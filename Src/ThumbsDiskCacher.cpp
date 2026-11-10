#include "ThumbsDiskCacher.h"
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#ifdef Q_OS_WIN
#include <windows.h>
#include <fileapi.h>
#endif

ThumbsDiskCacher::ThumbsDiskCacher(QObject *parent)
    : QObject{parent}
{

}

void ThumbsDiskCacher::storeThumbInCache(QString path, QImage thumb)
{
    if (lazy_) {
        loadThumbs(currentDirPathes_, currentSortedFiles_, false);
        lazy_= false;
    }
    if (!thumbs_.contains(path)) {
        thumbs_.insert(path, thumb);
        modified_ = true;
    }
}

void ThumbsDiskCacher::saveThumbs()
{
    // Save if modified, even if currentSortedFiles is empty
    // (might contain folder thumbnails or other important data)
    if (modified_) {
        // If no sorted files, there's nothing to save by path
        if (currentSortedFiles_.empty()) {
            modified_ = false;
            return;
        }
        //split thumbs by dir
        QHash<QString/*dirPath*/, QList<const ThumbData*>> thumbsByDirs;
        for (const ThumbData& td : qAsConst(currentSortedFiles_)) {
            if (td.noThumb)
                continue;
            QFileInfo fi(td.fileShortcutPath);
            QString path = fi.absolutePath();
            if (thumbsByDirs.contains(path)) {
                thumbsByDirs[path].append(&td);
            } else {
                thumbsByDirs.insert(path, {&td});
            }
        }
        //save thumbs for every folder in separate thumbsFile_
        for (auto iter = thumbsByDirs.cbegin(); iter != thumbsByDirs.cend(); iter ++) {
            QDir dir(iter.key());
            QString cacheFilePath = dir.filePath(thumbsFile_);
            QFile f(cacheFilePath);
            if (f.open(QIODevice::WriteOnly)) {
                QDataStream ds(&f);
                ds.setVersion(QDataStream::Qt_5_14);
                bool writeSuccess = true;
                
                // Write version first
                ds << version_;
                if (ds.status() != QDataStream::Ok) {
                    writeSuccess = false;
                }
                
                // Write thumbnails
                if (writeSuccess) {
                    for (const ThumbData* file : qAsConst(iter.value())) {
                        auto i = thumbs_.constFind(file->filePath);
                        if (i != thumbs_.constEnd() ) {
                            ds << file->filePath;
                            if (ds.status() != QDataStream::Ok) {
                                writeSuccess = false;
                                break;
                            }
                            if (!i->save(ds.device(), format_, quality_)) {
                                writeSuccess = false;
                                break;
                            }
                            if (ds.status() != QDataStream::Ok) {
                                writeSuccess = false;
                                break;
                            }
                        }
                    }
                }
                
                f.flush();
                f.close();
                
                // Only set hidden attribute if write was successful
                // If write failed, file might be corrupted - leave it visible for easier debugging
                if (writeSuccess) {
#ifdef Q_OS_WIN
                    SetFileAttributesA(cacheFilePath.toStdString().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
                } else {
                    // Remove corrupted file to prevent loading invalid data next time
                    f.remove();
                }
            }
        }
        modified_ = false;
    }

}

void ThumbsDiskCacher::loadThumbsInternal(QString dirPath, bool forFolder, bool *success)
{
    if (success) *success = false;
    QDir dir(dirPath);
    QFile f(dir.filePath(thumbsFile_));
    if (f.open(QIODevice::ReadOnly)) {
        QDataStream ds(&f);
        ds.setVersion(QDataStream::Qt_5_14);
        qint8 version;
        ds >> version;
        
        // If version doesn't match, skip this file (could be from different version)
        // Don't delete it - might be valid for another instance
        if (version != version_) {
            f.close();
            return;
        }
        
        // Try to load thumbnails, handle partial corruption gracefully
        while (!ds.atEnd()) {
            // Check for read errors before proceeding
            if (ds.status() != QDataStream::Ok) {
                break;
            }
            
            QString path;
            ds >> path;
            
            // Check for read errors or empty path
            if (ds.status() != QDataStream::Ok || path.isEmpty()) {
                break;
            }
            
            QImage thumb;
            // Validate that image was loaded successfully
            if (thumb.load(ds.device(), format_) && !thumb.isNull()) {
                if (forFolder) {
                    thumbs_.insert(dirPath, thumb);
                    emit thumbLoaded(dirPath, thumb);
                    if (success) *success = true;
                    break;
                } else {
                    thumbs_.insert(path, thumb);
                    emit thumbLoaded(path, thumb);
                    if (success) *success = true;
                }
            } else {
                // Skip corrupted thumbnail entry, continue with next
                // This allows partial recovery of cache file
                // If we hit end of stream or read error, break
                if (ds.status() != QDataStream::Ok) {
                    break;
                }
                continue;
            }
        }
    }
    f.close();
}

void ThumbsDiskCacher::loadThumbs(QStringList dirPathes, QList<ThumbData> sortedFiles, bool lazy)
{
    saveThumbs();
    thumbs_.clear();
    modified_ = false;
    currentSortedFiles_ = sortedFiles;
    currentDirPathes_ = dirPathes;
    lazy_ = lazy;
    if (lazy)  {
        return;
    }
    //load thumbs for files and dirs stored in dirPathes+thumbsFile_
    for (const QString& dirPath : qAsConst(dirPathes)) {
        loadThumbsInternal(dirPath, false);
    }
    //load thumbs for dirs without thumb from currentSortedFiles_.filePath+thumbsFile_
    for (const ThumbData& file : qAsConst(currentSortedFiles_)) {
        if (file.type == ThumbData::Folder
            && !file.noThumb
            && !thumbs_.contains(file.filePath))
        {
            bool success = false;
            loadThumbsInternal(file.filePath, true, &success);
            if (success) modified_ = true;
        }
    }
    emit allThumbsLoaded(thumbs_.keys());
}

QImage ThumbsDiskCacher::getThumb(QString path)
{
    if (lazy_) {
        loadThumbs(currentDirPathes_, currentSortedFiles_, false);
        lazy_= false;
    }
    return thumbs_.value(path);
}

bool ThumbsDiskCacher::haveThumb(QString path)
{
    if (lazy_) {
        loadThumbs(currentDirPathes_, currentSortedFiles_, false);
        lazy_= false;
    }
    return thumbs_.contains(path);
}

void ThumbsDiskCacher::setCurrentFolderThumb(QImage newThumb)
{
    //can't set thumb for united folder
    if (currentDirPathes_.size() != 1) return;

    QString currentDirPath = currentDirPathes_.first();
    QDir dir(currentDirPath);

    //can't set thumb for folder without parent
    if (!dir.cdUp()) return;

    //load thumbs for parent folder and repalce current folder thumb to newThumb
    QList<QPair<QString, QImage>> thumbs;
    QFile f(dir.filePath(thumbsFile_));
    bool replaced = false;
    if (f.open(QIODevice::ReadOnly)) {
        QDataStream ds(&f);
        ds.setVersion(QDataStream::Qt_5_14);
        qint8 version;
        ds >> version;
        if (version == version_) {
            while (!ds.atEnd()) {
                QString path;
                ds >> path;
                if (path.isEmpty()) break;
                QImage oldThumb;
                oldThumb.load(ds.device(), format_);
                if (path == currentDirPath) {
                    replaced = true;
                    thumbs.append({path, newThumb});
                } else {
                    thumbs.append({path, oldThumb});
                }
            }
        }
        f.close();
    }
    if (!replaced) {
        thumbs.append({currentDirPath, newThumb});
    }

    //save thumbs with new one for folder
    if (f.open(QIODevice::WriteOnly)) {
        QDataStream ds(&f);
        ds.setVersion(QDataStream::Qt_5_14);
        bool writeSuccess = true;
        
        ds << version_;
        if (ds.status() != QDataStream::Ok) {
            writeSuccess = false;
        }
        
        if (writeSuccess) {
            for (auto& thumb : qAsConst(thumbs)) {
                ds << thumb.first;
                if (ds.status() != QDataStream::Ok) {
                    writeSuccess = false;
                    break;
                }
                if (!thumb.second.save(ds.device(), format_, quality_)) {
                    writeSuccess = false;
                    break;
                }
                if (ds.status() != QDataStream::Ok) {
                    writeSuccess = false;
                    break;
                }
            }
        }
        
        f.flush();
        f.close();
        
        if (writeSuccess) {
            emit thumbLoaded(currentDirPath, newThumb);
        }
    }
}

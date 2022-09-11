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
    if (modified_ && !currentSortedFiles_.empty()) {
        //split thumbs by dir
        QHash<QString/*dirPath*/, QList<const ThumbData*>> thumbsByDirs;
        for (const ThumbData& td : qAsConst(currentSortedFiles_)) {
            if (td.noThumb)
                continue;
            QFileInfo fi(td.filePath);
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
            QFile f(dir.filePath(thumbsFile_));
            if (f.open(QIODevice::WriteOnly)) {
                QDataStream ds(&f);
                ds.setVersion(QDataStream::Qt_5_14);
                ds << version_;
                for (const ThumbData* file : qAsConst(iter.value())) {
                    auto i = thumbs_.constFind(file->filePath);
                    if (i != thumbs_.constEnd() ) {
                        ds << file->filePath;
                        i->save(ds.device(), format_, quality_);
                    }
                }
                f.close();
#ifdef Q_OS_WIN
                SetFileAttributesA(dir.filePath(thumbsFile_).toStdString().c_str(), FILE_ATTRIBUTE_HIDDEN);
#endif
            }
        }
        modified_ = false;
    }

}

void ThumbsDiskCacher::loadThumbsInternal(QString dirPath, bool forFolder, bool *success)
{
    if (success) *success = false;
    QFile f(dirPath + "\\" + thumbsFile_);
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
                QImage thumb;
                thumb.load(ds.device(), format_);
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
    QFile f(dir.absolutePath() + "\\" + thumbsFile_);
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
        ds << version_;
        for (auto& thumb : qAsConst(thumbs)) {
            ds << thumb.first;
            thumb.second.save(ds.device(), format_, quality_);
        }
        f.close();
        emit thumbLoaded(currentDirPath, newThumb);
    }
}

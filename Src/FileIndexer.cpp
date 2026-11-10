#include "FileIndexer.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutexLocker>

const QSet<QString> FileIndexer::videoExt_ = {"mp4", "mpg", "wmv", "mkv", "ts", "avi", "mov", "m2ts", "flv"};
const QSet<QString> FileIndexer::pictExt_ = {"jpg", "jpeg", "png", "jfif"};
const QSet<QString> FileIndexer::linkExt_ = {"lnk"};

FileIndexer::FileIndexer(QObject *parent)
    : QObject(parent)
    , indexingWatcher_(nullptr)
    , isIndexing_(false)
{
    indexingWatcher_ = new QFutureWatcher<void>(this);
    connect(indexingWatcher_, &QFutureWatcher<void>::finished, this, &FileIndexer::onIndexingFinished);
}

FileIndexer::~FileIndexer()
{
    if (indexingWatcher_) {
        indexingWatcher_->waitForFinished();
    }
}

void FileIndexer::indexFolders(const QList<FavoriteFolderData>& folders)
{
    if (isIndexing_) {
        return; // Already indexing
    }
    
    isIndexing_ = true;
    emit indexingStarted();
    emit indexingStateChanged();
    
    // Collect all unique folder paths
    QSet<QString> folderPaths;
    for (const FavoriteFolderData& folder : folders) {
        for (const QString& path : folder.pathes) {
            QDir dir(path);
            if (dir.exists()) {
                folderPaths.insert(dir.absolutePath());
            }
        }
    }
    
    // Start indexing in background thread
    QFuture<void> future = QtConcurrent::run([this, folderPaths]() {
        QMutexLocker lock(&indexMutex_);
        
        indexedFiles_.clear();
        folderIndex_.clear();
        
        for (const QString& folderPath : folderPaths) {
            QList<IndexedFile> folderFiles;
            indexFolderRecursive(folderPath, folderFiles);
            
            folderIndex_.insert(folderPath, folderFiles);
            indexedFiles_.append(folderFiles);
            
            emit folderIndexed(folderPath, folderFiles.size());
        }
    });
    
    indexingWatcher_->setFuture(future);
}

void FileIndexer::indexFolderRecursive(const QString& folderPath, QList<IndexedFile>& files)
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        return;
    }
    
    // Get all files and directories
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    for (const QFileInfo& entry : entries) {
        if (entry.isDir()) {
            // Recursively index subdirectories
            indexFolderRecursive(entry.absoluteFilePath(), files);
        } else if (entry.isFile()) {
            // Check if file should be indexed
            if (shouldIndexFile(entry)) {
                IndexedFile indexedFile;
                indexedFile.filePath = entry.absoluteFilePath();
                indexedFile.displayName = entry.fileName();
                indexedFile.directory = entry.absolutePath();
                indexedFile.type = getFileType(entry);
                indexedFile.size = entry.size();
                
                files.append(indexedFile);
            }
        }
    }
}

bool FileIndexer::shouldIndexFile(const QFileInfo& fileInfo) const
{
    QString ext = fileInfo.suffix().toLower();
    return videoExt_.contains(ext) || pictExt_.contains(ext) || linkExt_.contains(ext);
}

ThumbData::Type FileIndexer::getFileType(const QFileInfo& fileInfo) const
{
    if (fileInfo.isDir()) {
        return ThumbData::Folder;
    }
    QString ext = fileInfo.suffix().toLower();
    if (videoExt_.contains(ext)) {
        return ThumbData::Video;
    }
    if (pictExt_.contains(ext)) {
        return ThumbData::Picture;
    }
    return ThumbData::Unsupported;
}

QList<IndexedFile> FileIndexer::searchFiles(const QString& searchText, int maxResults) const
{
    QMutexLocker lock(&indexMutex_);
    
    QList<IndexedFile> results;
    if (searchText.isEmpty()) {
        return results;
    }
    
    QString lowerSearch = searchText.toLower();
    
    for (const IndexedFile& file : indexedFiles_) {
        if (results.size() >= maxResults) {
            break;
        }
        
        QString lowerName = file.displayName.toLower();
        QString lowerPath = file.filePath.toLower();
        
        // Search in file name and path
        if (lowerName.contains(lowerSearch) || lowerPath.contains(lowerSearch)) {
            results.append(file);
        }
    }
    
    return results;
}

QList<IndexedFile> FileIndexer::getAllFiles() const
{
    QMutexLocker lock(&indexMutex_);
    return indexedFiles_;
}

void FileIndexer::clearIndex()
{
    QMutexLocker lock(&indexMutex_);
    indexedFiles_.clear();
    folderIndex_.clear();
    emit indexUpdated();
}

void FileIndexer::updateFolder(const QString& folderPath)
{
    if (isIndexing_) {
        return;
    }
    
    QDir dir(folderPath);
    if (!dir.exists()) {
        return;
    }
    
    // Remove old files from this folder
    {
        QMutexLocker lock(&indexMutex_);
        auto it = indexedFiles_.begin();
        while (it != indexedFiles_.end()) {
            if (it->directory == folderPath || it->filePath.startsWith(folderPath)) {
                it = indexedFiles_.erase(it);
            } else {
                ++it;
            }
        }
        folderIndex_.remove(folderPath);
    }
    
    // Index folder in background
    QFuture<void> future = QtConcurrent::run([this, folderPath]() {
        QList<IndexedFile> folderFiles;
        indexFolderRecursive(folderPath, folderFiles);
        
        QMutexLocker lock(&indexMutex_);
        folderIndex_.insert(folderPath, folderFiles);
        indexedFiles_.append(folderFiles);
        
        emit folderIndexed(folderPath, folderFiles.size());
        emit indexUpdated();
    });
    
    indexingWatcher_->setFuture(future);
}

void FileIndexer::onIndexingFinished()
{
    isIndexing_ = false;
    emit indexingFinished();
    emit indexingStateChanged();
    emit indexUpdated();
}

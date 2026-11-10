#ifndef FILEINDEXER_H
#define FILEINDEXER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QThread>
#include <QSet>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include "ThumbData.h"
#include "FavoriteFolderData.h"

struct IndexedFile {
    QString filePath;
    QString displayName;
    QString directory;
    ThumbData::Type type;
    qint64 size;
    
    bool operator==(const IndexedFile& other) const {
        return filePath == other.filePath;
    }
};

class FileIndexer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isIndexing READ isIndexing NOTIFY indexingStateChanged)
    Q_PROPERTY(int indexedFilesCount READ indexedFilesCount NOTIFY indexUpdated)
    
public:
    explicit FileIndexer(QObject *parent = nullptr);
    ~FileIndexer();
    
    // Start indexing all folders from favorite folders list
    Q_INVOKABLE void indexFolders(const QList<FavoriteFolderData>& folders);
    
    // Search files by text (searches in file names)
    Q_INVOKABLE QList<IndexedFile> searchFiles(const QString& searchText, int maxResults = 100) const;
    
    // Get all indexed files
    Q_INVOKABLE QList<IndexedFile> getAllFiles() const;
    
    // Clear index
    Q_INVOKABLE void clearIndex();
    
    // Update index for specific folder
    Q_INVOKABLE void updateFolder(const QString& folderPath);
    
    bool isIndexing() const { return isIndexing_; }
    int indexedFilesCount() const { return indexedFiles_.size(); }
    
signals:
    void indexingStarted();
    void indexingFinished();
    void indexingStateChanged();
    void indexUpdated();
    void folderIndexed(const QString& folderPath, int fileCount);
    
private slots:
    void onIndexingFinished();
    
private:
    void indexFolderRecursive(const QString& folderPath, QList<IndexedFile>& files);
    bool shouldIndexFile(const QFileInfo& fileInfo) const;
    ThumbData::Type getFileType(const QFileInfo& fileInfo) const;
    
    QList<IndexedFile> indexedFiles_;
    QHash<QString, QList<IndexedFile>> folderIndex_; // folder path -> files
    mutable QMutex indexMutex_;
    
    QFutureWatcher<void>* indexingWatcher_;
    volatile bool isIndexing_;
    
    // File extensions to index (same as in DirProcessor)
    static const QSet<QString> videoExt_;
    static const QSet<QString> pictExt_;
    static const QSet<QString> linkExt_;
};

Q_DECLARE_METATYPE(IndexedFile)

#endif // FILEINDEXER_H

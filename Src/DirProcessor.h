#ifndef DIRPROCESSOR_H
#define DIRPROCESSOR_H

#include <QList>
#include <QMutexLocker>
#include <QObject>
#include <QtConcurrent/QtConcurrent>

#include "ThumbData.h"
#include "ThumbGenerators/VideoThumbnailGeneratorInterface.h"
#include "ThumbGenerators/VideoThumbnailGeneratorFactory.h"
#include "ThumbsProviderInterface.h"

class DirProcessor : public QObject
{
    Q_OBJECT
public:
    DirProcessor(ThumbsProviderInterface*, VideoThumbnailGeneratorFactory&);

    bool getUnitedDirFiles(QStringList dirPathes, QList<ThumbData>& thumbsOut, bool *allHaveThumbs);

    //emits getDirFilesAsyncResponce
    void getUnitedDirFilesAsync(QStringList dirPathes);

    //generate thumbnails for dirs passed to "getUnitedDirFilesAsync" excluding "excludePathes"
    //emits "fileProcessed" for every created thumb and "allFilesProcessed" at finish
    void startGeneratingThumbnails(QStringList excludePathes);

    //generate one thumbnail for path, emits "fileProcessed"
    void generateThumbnail(QString path);

    //all created thumb sizes multiply to scale
    void setScale(double scale) {scale_ = scale;}

    //cancel generating thumbnails
    void cancel();    
    void waitFinished();

signals:
    void fileProcessed(QString filePath, QString thumbSource, bool success);
    void allFilesProcessed(QList<ThumbData> files);
    void getDirFilesAsyncResponce(QStringList dirPathes, QList<ThumbData> sortedDirFiles, bool success, bool allHaveThumbs);

protected:
    QImage makeThumb(ThumbData thumbData);
    static ThumbData::Type getFileType(const QFileInfo& fileInfo);
    static QFileInfo getMostSuitable(const QFileInfoList& folderFiles);

    ThumbsProviderInterface* thumbsProvider_;
    VideoThumbnailGeneratorFactory& videoThumbnailGeneratorFactory_;
    QList<ThumbData> thumbsForGeneration_;
    QFutureWatcher<QImage> *thumbsCreating_ = nullptr;

    QHash<QString, std::shared_ptr<VideoThumbnailGeneratorInterface>> thumbnailGenerators_;
    QMutex thumbnailGeneratorsMutex_;

    QSet<QString> pathesProcessing_;
    QMutex pathesProcessingMutex_;

    QSize thumbSize_ = {100, 100};
    double scale_ = 1;
    volatile bool canceled_ = false;

    inline static const QSet<QString> videoExt_ = {"mp4", "mpg", "wmv", "mkv", "ts", "avi", "mov", "m2ts", "flv"};
    inline static const QSet<QString> pictExt_ = {"jpg", "jpeg", "png", "jfif"};
    inline static const QSet<QString> linkExt_ = {"lnk"};
};

#endif // DIRPROCESSOR_H

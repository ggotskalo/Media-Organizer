#include "DirProcessor.h"
#include <QDir>
#include <QImageReader>
#include <QMetaObject>
#include <algorithm>

DirProcessor::DirProcessor(ThumbsProvider* thumbsProvider) : thumbsProvider_(thumbsProvider)
{    
    thumbsCreating_ = new QFutureWatcher<QImage>(this);
    connect(thumbsCreating_, &QFutureWatcher<QImage>::resultReadyAt, this, [this](int num) {
        QImage thumb = thumbsCreating_->resultAt(num);
        const ThumbData& td = thumbsForGeneration_.at(num);
        QString path = td.filePath;
        pathesProcessing_.remove(path);
        if (!thumb.isNull()) {
            thumbsProvider_->addImage(td.thumbSource, thumb);
            if (!thumbsCreating_->isCanceled()) {
                emit fileProcessed(path, td.thumbSource, true);
            }
        }
    });
    connect(thumbsCreating_, &QFutureWatcher<QImage>::finished, this, [this]() {
                emit allFilesProcessed(thumbsForGeneration_);
        }
    );

}

void DirProcessor::startGeneratingThumbnails(QStringList excludePathes)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, excludePathes]{startGeneratingThumbnails(excludePathes);}, Qt::QueuedConnection);
        return;
    }

    cancel();
    canceled_ = false;
    QSet<QString> excludePathesSet(excludePathes.begin(), excludePathes.end());
    auto newEnd = std::remove_if(thumbsForGeneration_.begin(), thumbsForGeneration_.end(), [&](const ThumbData& td){
       return td.noThumb || excludePathesSet.contains(td.filePath);
    });
    thumbsForGeneration_.erase(newEnd, thumbsForGeneration_.end());
    thumbsCreating_->setFuture(QtConcurrent::mapped(thumbsForGeneration_, std::bind(&DirProcessor::makeThumb,
                                                                  this,
                                                                  std::placeholders::_1)));
}

void DirProcessor::generateThumbnail(QString path)
{
    {
        QMutexLocker lock(&pathesProcessingMutex_);
        if (pathesProcessing_.contains(path)) {
            return;
        }
    }
    QFileInfo fi(path);
    QString file = fi.fileName();
    ThumbData thumbData = {file, path, getFileType(fi), path, 0};
    emit fileProcessed(path, "", true);
    pathesProcessing_.insert(path);
    QtConcurrent::run([this, thumbData, path]{
        QImage thumb = makeThumb(thumbData);
        if (!thumb.isNull()) {
            thumbsProvider_->addImage(path, thumb);
            emit fileProcessed(path, path, true);
        }
        {
            QMutexLocker lock(&pathesProcessingMutex_);
            pathesProcessing_.remove(path);
        }
    });
}

void DirProcessor::cancel() {
    thumbsCreating_->cancel();
    canceled_ = true;
    {
        QMutexLocker lock(&thumbnailGeneratorsMutex_);
        for (VideoThumbnailGeneratorWin32* thumbnailGenerator : qAsConst(thumbnailGenerators_)) {
            thumbnailGenerator->abort();
        }
    }
}

void DirProcessor::waitFinished()
{
    thumbsCreating_->waitForFinished();
}

bool DirProcessor::getUnitedDirFiles(QStringList dirPathes, QList<ThumbData> &thumbsOut, bool *allHaveThumbs)
{
    if (allHaveThumbs) *allHaveThumbs = true;
    cancel();

    QStringList filter;
    foreach (const QString& ext, videoExt_ + pictExt_ + linkExt_) {
        filter << "*." + ext;
    }

    QFileInfoList files;
    for (const QString& dirPath : qAsConst(dirPathes)) {
        QDir currentDir;
        currentDir.setPath(dirPath);
        files.append(currentDir.entryInfoList({},
                                                       QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot,
                                                       QDir::Unsorted));
    }

    std::stable_sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b){
        return a.isDir() == b.isDir() ?
               a.fileName().toLower() < b.fileName().toLower() :
               a.isDir() > b.isDir();
    });

    thumbsOut.clear();
    thumbsOut.reserve(files.count());
    thumbsForGeneration_.clear();
    thumbsForGeneration_.reserve(files.count());

    //for simple folder with parent add parent folder shortcut
    if (dirPathes.size() == 1) {
        QDir dir(dirPathes.first());
        if (dir.cdUp()) {
            thumbsOut.prepend({"..", dir.absolutePath(), ThumbData::Folder, "", 0, true});
        }
    }

    for (QFileInfo& fileInfo : files) {
        if (fileInfo.isShortcut()) {
            fileInfo.setFile(fileInfo.symLinkTarget());
        }
    }
    for (const QFileInfo& fileInfo : qAsConst(files)) {

        ThumbData::Type type = getFileType(fileInfo);
        QString name = fileInfo.fileName();
        QString path = fileInfo.absoluteFilePath();

        if (thumbsProvider_->haveThumb(path)) {

            thumbsOut.append({name, path, type, path, fileInfo.size()});

        } else {

            thumbsOut.append({name, path, type, "", fileInfo.size()});

            if (type == ThumbData::Folder) {
                //for folder without thumb make thumb from first suitable file from folder
                QDir dir(path);
                QFileInfoList folderFiles = dir.entryInfoList(filter,
                                                       QDir::Files, QDir::Name);
                if (!folderFiles.isEmpty()) {
                    for (QFileInfo& folderFile : folderFiles) {
                        if (folderFile.isShortcut()) {
                            folderFile.setFile(folderFile.symLinkTarget());
                        }
                    }
                    QFileInfo found = getMostSuitable(folderFiles);
                    ThumbData::Type foundThumbType = getFileType(found);
                    thumbsForGeneration_.append({name, path, foundThumbType, found.absoluteFilePath(), fileInfo.size()});
                }
            } else {
                thumbsForGeneration_.append({name, path, type, path, fileInfo.size()});
            }
            if (allHaveThumbs) *allHaveThumbs = false;

        }
    }
    return true;
}

QFileInfo DirProcessor::getMostSuitable(const QFileInfoList& folderFiles)
{
    static const QSet<QString> highPriorityNames = {"cover"};
    static const QSet<QString> lowPriorityNames = {"contactsheet"};

    QFileInfo found, lowPriority;
    for (auto& folderFile : folderFiles) {
        if (highPriorityNames.contains(folderFile.baseName().toLower())) {
            found = folderFile;
            break;
        }
        if (lowPriorityNames.contains(folderFile.baseName().toLower())) {
            if (lowPriority.fileName().isEmpty()) {
                lowPriority = folderFile;
            }
            continue;
        }
        if (found.baseName().isEmpty()) {
            found = folderFile;
        }
    }
    if (found.baseName().isEmpty()) {
            found = lowPriority;
    }
    return found;
}

void DirProcessor::getUnitedDirFilesAsync(QStringList dirPathes)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, dirPathes]{getUnitedDirFilesAsync(dirPathes);}, Qt::QueuedConnection);
        return;
    }
    QList<ThumbData> thumbsOut;
    bool allHaveThumbs;
    bool ret = getUnitedDirFiles(dirPathes, thumbsOut, &allHaveThumbs);
    emit getDirFilesAsyncResponce(dirPathes, thumbsOut, ret, allHaveThumbs);
}

ThumbData::Type DirProcessor::getFileType(const QFileInfo& fileInfo)
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

QImage DirProcessor::makeThumb(ThumbData fileData)
{
    if (fileData.type == ThumbData::Picture) {

        QImageReader image(fileData.thumbSource);
        QSize size = image.size();
        size.scale(thumbSize_, Qt::KeepAspectRatio);
        image.setScaledSize(size * scale_);
        image.setAutoTransform(true);
        if (canceled_) {
            return QImage();
        }
        return image.read();

    } else if (fileData.type == ThumbData::Video) {

        VideoThumbnailGeneratorWin32 thumbnailGenerator;
        {
            QMutexLocker lock(&thumbnailGeneratorsMutex_);
            thumbnailGenerators_.insert(fileData.thumbSource, &thumbnailGenerator);
        }
        QImage images[1];
        thumbnailGenerator.getThumbs(fileData.thumbSource.toStdWString().c_str(), 1, images);
        {
            QMutexLocker lock(&thumbnailGeneratorsMutex_);
            thumbnailGenerators_.remove(fileData.thumbSource);
        }
        return images[0];

    }

    return QImage();
}

#include "ThumbsProvider.h"

#include <QDebug>
#include <QUrl>

ThumbsProvider::ThumbsProvider(): QQuickImageProvider(QQuickImageProvider::Image), cache_(1000)
{

}

QImage ThumbsProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage image;
    QString path = QUrl::fromPercentEncoding(id.toUtf8());
    {
        QMutexLocker lock(&cacheMutex_);
        QImage *cachedImage = cache_.object(path);
        if (cachedImage) {
            image = *cachedImage;
        }
    }
    if (!image.isNull()) {
        if (size) {
            *size = image.size();
#ifdef QT_DEBUG
            if ( std::max(requestedSize.width(), requestedSize.height()) !=
                 std::max(size->width(), size->height()) )
            {
                qWarning()<<"Wrong thumb size in cache. Requested, have:" << requestedSize << *size;
            }
#endif
        }
    } else {
        emit thumbRequired(path);
    }
    return image;
}

QImage ThumbsProvider::getImage(const QString &id) const
{
    QMutexLocker lock(&cacheMutex_);
    QImage *cachedImage = cache_.object(id);
    if (cachedImage) {
        return QImage(*cachedImage);
    }
    return QImage();
}

void ThumbsProvider::addImage(const QString path, const QImage &image)
{
    QMutexLocker lock(&cacheMutex_);
    cache_.insert(path, new QImage(image));
}

bool ThumbsProvider::haveThumb(const QString path) const
{
    QMutexLocker lock(&cacheMutex_);
    return cache_.contains(path);
}

#include "ViewerImageCache.h"

#include <QImageReader>
#include <QThread>
#include <QUrl>

namespace {
const int kMaxCachedImages = 6;

struct ParsedId {
    QString path;
    int width = 0;
};

ParsedId parseViewerId(const QString &id)
{
    ParsedId parsed;
    const QString text = QUrl::fromPercentEncoding(id.toUtf8());
    const int sep = text.indexOf(QLatin1Char('/'));
    if (sep <= 0)
        return parsed;

    bool ok = false;
    parsed.width = text.left(sep).toInt(&ok);
    if (!ok || parsed.width <= 0)
        parsed.width = 0;
    parsed.path = text.mid(sep + 1);
    return parsed;
}
}

ViewerImageCache::ViewerImageCache(QObject *parent)
    : QObject(parent)
{
    decodeThread_ = QThread::create([this] { workerLoop(); });
    decodeThread_->setObjectName(QStringLiteral("ViewerImageDecode"));
    decodeThread_->start();
}

ViewerImageCache::~ViewerImageCache()
{
    {
        QMutexLocker lock(&mutex_);
        stop_ = true;
        queue_.clear();
    }
    work_.wakeAll();
    ready_.wakeAll();
    if (decodeThread_) {
        decodeThread_->wait();
        delete decodeThread_;
        decodeThread_ = nullptr;
    }
}

QString ViewerImageCache::cacheKey(const QString &path, int width)
{
    return path + QLatin1Char('\n') + QString::number(width);
}

QImage ViewerImageCache::decode(const QString &path, int targetWidth)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    // Fast DCT and cheap resampling. Browsing wants the next frame now.
    reader.setQuality(40);
    const QSize original = reader.size();
    if (targetWidth > 0 && original.width() > targetWidth) {
        const int height = qMax(1, qRound(original.height() * (qreal(targetWidth) / original.width())));
        reader.setScaledSize(QSize(targetWidth, height));
    }
    QImage image = reader.read();
    if (image.isNull())
        return image;
    if (image.format() != QImage::Format_RGB32 && image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(image.hasAlphaChannel()
                                       ? QImage::Format_ARGB32_Premultiplied
                                       : QImage::Format_RGB32);
    }
    return image;
}

void ViewerImageCache::enqueueFrontLocked(const QString &path, int width)
{
    const QString id = cacheKey(path, width);
    if (images_.contains(id))
        return;
    if (activePath_ == path && activeWidth_ == width)
        return;
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_.at(i).path == path && queue_.at(i).width == width)
            queue_.removeAt(i);
    }
    queue_.prepend(Request{path, width});
    work_.wakeOne();
}

void ViewerImageCache::refillQueueLocked(const QStringList &paths, int width)
{
    queue_.clear();
    const auto consider = [this](const QString &path, int w) {
        if (path.isEmpty() || w <= 0)
            return;
        const QString id = cacheKey(path, w);
        if (images_.contains(id))
            return;
        if (activePath_ == path && activeWidth_ == w)
            return;
        for (const Request &queued : qAsConst(queue_)) {
            if (queued.path == path && queued.width == w)
                return;
        }
        queue_.append(Request{path, w});
    };
    for (const Request &waiting : qAsConst(waited_))
        consider(waiting.path, waiting.width);
    for (const QString &path : paths)
        consider(path, width);
    work_.wakeOne();
}

void ViewerImageCache::touchLruLocked(const QString &id)
{
    lru_.removeAll(id);
    lru_.append(id);
}

void ViewerImageCache::trimLocked()
{
    while (lru_.size() > kMaxCachedImages) {
        const QString old = lru_.takeFirst();
        images_.remove(old);
    }
}

void ViewerImageCache::prefetch(const QStringList &paths, int width)
{
    if (width <= 0)
        return;

    QMutexLocker lock(&mutex_);
    queue_.clear();
    for (const QString &path : paths) {
        if (path.isEmpty())
            continue;
        failed_.remove(cacheKey(path, width));
    }
    refillQueueLocked(paths, width);
}

void ViewerImageCache::bump(const QString &path, int width)
{
    if (path.isEmpty() || width <= 0)
        return;

    QMutexLocker lock(&mutex_);
    failed_.remove(cacheKey(path, width));
    enqueueFrontLocked(path, width);
}

QImage ViewerImageCache::waitFor(const QString &path, int width)
{
    if (path.isEmpty() || width <= 0)
        return {};

    const QString id = cacheKey(path, width);
    QMutexLocker lock(&mutex_);
    waited_.append(Request{path, width});
    failed_.remove(id);
    enqueueFrontLocked(path, width);
    while (!stop_ && !images_.contains(id) && !failed_.contains(id))
        ready_.wait(&mutex_);
    for (int i = 0; i < waited_.size(); ++i) {
        if (waited_.at(i).path == path && waited_.at(i).width == width) {
            waited_.removeAt(i);
            break;
        }
    }
    if (stop_ || failed_.contains(id))
        return {};
    touchLruLocked(id);
    return images_.value(id);
}

void ViewerImageCache::workerLoop()
{
    for (;;) {
        Request job;
        {
            QMutexLocker lock(&mutex_);
            while (!stop_ && queue_.isEmpty())
                work_.wait(&mutex_);
            if (stop_)
                return;
            job = queue_.takeFirst();
            activePath_ = job.path;
            activeWidth_ = job.width;
        }

        const QImage image = decode(job.path, job.width);

        {
            QMutexLocker lock(&mutex_);
            if (activePath_ == job.path && activeWidth_ == job.width) {
                activePath_.clear();
                activeWidth_ = 0;
            }
            const QString id = cacheKey(job.path, job.width);
            if (image.isNull()) {
                failed_.insert(id);
            } else {
                images_.insert(id, image);
                touchLruLocked(id);
                trimLocked();
            }
        }
        ready_.wakeAll();
    }
}

ViewerImageProvider::ViewerImageProvider(ViewerImageCache *cache)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , cache_(cache)
{
}

QImage ViewerImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    const ParsedId parsed = parseViewerId(id);
    const QImage image = cache_->waitFor(parsed.path, parsed.width);
    if (size)
        *size = image.size();
    return image;
}

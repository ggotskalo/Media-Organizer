#ifndef VIEWERIMAGECACHE_H
#define VIEWERIMAGECACHE_H

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QSet>
#include <QStringList>
#include <QWaitCondition>

class QThread;

// Decodes pictures down to the screen size on a background thread and keeps
// the current image plus its neighbors. The viewer then flips to a frame that
// is already in memory.
class ViewerImageCache : public QObject
{
    Q_OBJECT
public:
    explicit ViewerImageCache(QObject *parent = nullptr);
    ~ViewerImageCache() override;

    Q_INVOKABLE void prefetch(const QStringList &paths, int width);
    Q_INVOKABLE void bump(const QString &path, int width);

    QImage waitFor(const QString &path, int width);

private:
    struct Request {
        QString path;
        int width = 0;
    };

    static QString cacheKey(const QString &path, int width);
    static QImage decode(const QString &path, int targetWidth);

    void workerLoop();
    void enqueueFrontLocked(const QString &path, int width);
    void refillQueueLocked(const QStringList &paths, int width);
    void touchLruLocked(const QString &id);
    void trimLocked();

    QThread *decodeThread_ = nullptr;
    QMutex mutex_;
    QWaitCondition work_;
    QWaitCondition ready_;
    bool stop_ = false;
    QList<Request> queue_;
    QList<Request> waited_;
    QString activePath_;
    int activeWidth_ = 0;
    QHash<QString, QImage> images_;
    QSet<QString> failed_;
    QStringList lru_;
};

class ViewerImageProvider : public QQuickImageProvider
{
public:
    explicit ViewerImageProvider(ViewerImageCache *cache);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ViewerImageCache *cache_;
};

#endif // VIEWERIMAGECACHE_H

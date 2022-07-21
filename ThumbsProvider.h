#ifndef THUMBSPROVIDER_H
#define THUMBSPROVIDER_H

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>

class ThumbsProvider : public QObject, public QQuickImageProvider
{
Q_OBJECT
public:
    ThumbsProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    QImage getImage(const QString &id) const;
    void addImage(const QString path, const QImage &image);
    bool haveThumb(const QString path) const;
signals:
    void thumbRequired(const QString path);

protected:
    QCache<QString, QImage> cache_;
    mutable QMutex cacheMutex_;

};

#endif // THUMBSPROVIDER_H

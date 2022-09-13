#ifndef THUMBSPROVIDER_H
#define THUMBSPROVIDER_H

#include "ThumbsProviderInterface.h"

#include <QQuickImageProvider>
#include <QCache>
#include <QImage>
#include <QMutex>

class ThumbsProvider : public QObject, public QQuickImageProvider, public ThumbsProviderInterface
{
Q_OBJECT
public:
    ThumbsProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    QImage getImage(const QString &id) const override;
    void addImage(const QString path, const QImage &image) override;
    bool haveThumb(const QString path) const override;
signals:
    void thumbRequired(const QString path);

protected:
    QCache<QString, QImage> cache_;
    mutable QMutex cacheMutex_;

};

#endif // THUMBSPROVIDER_H

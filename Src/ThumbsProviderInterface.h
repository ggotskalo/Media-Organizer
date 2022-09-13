#ifndef THUMBSPROVIDERINTERFACE_H
#define THUMBSPROVIDERINTERFACE_H
class QImage;
class QString;
class ThumbsProviderInterface {
public:
    virtual QImage getImage(const QString &id) const = 0;
    virtual void addImage(const QString path, const QImage &image) = 0;
    virtual bool haveThumb(const QString path) const = 0;
    virtual ~ThumbsProviderInterface(){};
};

#endif // THUMBSPROVIDERINTERFACE_H

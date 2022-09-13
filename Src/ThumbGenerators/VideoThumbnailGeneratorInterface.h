#ifndef VIDEOTHUMBNAILGENERATORINTERFACE_H
#define VIDEOTHUMBNAILGENERATORINTERFACE_H
class QString;
class QImage;
class VideoThumbnailGeneratorInterface {
public:
    virtual bool getThumbs(QString path, int count, QImage images[]) = 0;
    virtual void abort() = 0;
    virtual ~VideoThumbnailGeneratorInterface() {};
};

#endif // VIDEOTHUMBNAILGENERATORINTERFACE_H

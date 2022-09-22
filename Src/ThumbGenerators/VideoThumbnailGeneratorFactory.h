#ifndef VIDEOTHUMBNAILGENERATORFACTORY_H
#define VIDEOTHUMBNAILGENERATORFACTORY_H
#include "VideoThumbnailGeneratorInterface.h"
class VideoThumbnailGeneratorFactory {
public:
    virtual VideoThumbnailGeneratorInterface* createGenerator() = 0;
    virtual ~VideoThumbnailGeneratorFactory(){}
};

#endif // VIDEOTHUMBNAILGENERATORFACTORY_H

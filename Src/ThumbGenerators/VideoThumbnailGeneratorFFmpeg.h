#pragma once

#include <QImage>
#include <QString>
#include <QMutex>
#include "VideoThumbnailGeneratorInterface.h"
#include "VideoThumbnailGeneratorFactory.h"

#ifdef HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class VideoThumbnailGeneratorFFmpeg : public VideoThumbnailGeneratorInterface
{
public:
    VideoThumbnailGeneratorFFmpeg();
    ~VideoThumbnailGeneratorFFmpeg() override;

    bool getThumbs(QString path, int count, QImage images[]) override;
    void abort() override;

private:
    bool openFile(const QString& filePath);
    bool seekToPosition(int64_t timestamp);
    bool decodeFrame(AVFrame* frame);
    QImage frameToQImage(AVFrame* frame);
    void cleanup();

    AVFormatContext* formatContext_;
    AVCodecContext* codecContext_;
    AVFrame* frame_;
    AVFrame* frameRGB_;
    struct SwsContext* swsContext_;
    uint8_t* rgbBuffer_;
    int videoStreamIndex_;
    
    QMutex mutex_{QMutex::Recursive};
    volatile bool aborted_;
    
    static bool ffmpegInitialized_;
    static void initializeFFmpeg();
};

class VideoThumbnailGeneratorFFmpegFactory : public VideoThumbnailGeneratorFactory {
public:
    VideoThumbnailGeneratorInterface *createGenerator() override
    {
        return new VideoThumbnailGeneratorFFmpeg();
    }
};

#else

// Stub implementation when FFmpeg is not available
class VideoThumbnailGeneratorFFmpeg : public VideoThumbnailGeneratorInterface
{
public:
    VideoThumbnailGeneratorFFmpeg() {}
    ~VideoThumbnailGeneratorFFmpeg() override {}
    bool getThumbs(QString path, int count, QImage images[]) override { 
        Q_UNUSED(path); Q_UNUSED(count); Q_UNUSED(images);
        return false; 
    }
    void abort() override {}
};

class VideoThumbnailGeneratorFFmpegFactory : public VideoThumbnailGeneratorFactory {
public:
    VideoThumbnailGeneratorInterface *createGenerator() override
    {
        return new VideoThumbnailGeneratorFFmpeg();
    }
};

#endif // HAVE_FFMPEG

#include "VideoThumbnailGeneratorFFmpeg.h"
#include <QDebug>
#include <QFileInfo>

#ifdef HAVE_FFMPEG

bool VideoThumbnailGeneratorFFmpeg::ffmpegInitialized_ = false;

VideoThumbnailGeneratorFFmpeg::VideoThumbnailGeneratorFFmpeg()
    : formatContext_(nullptr)
    , codecContext_(nullptr)
    , frame_(nullptr)
    , frameRGB_(nullptr)
    , swsContext_(nullptr)
    , rgbBuffer_(nullptr)
    , videoStreamIndex_(-1)
    , aborted_(false)
{
    initializeFFmpeg();
}

VideoThumbnailGeneratorFFmpeg::~VideoThumbnailGeneratorFFmpeg()
{
    cleanup();
}

void VideoThumbnailGeneratorFFmpeg::initializeFFmpeg()
{
    if (!ffmpegInitialized_) {
        // FFmpeg 4.0+ doesn't require explicit initialization
        // But we can set log level if needed
        av_log_set_level(AV_LOG_ERROR);
        ffmpegInitialized_ = true;
    }
}

void VideoThumbnailGeneratorFFmpeg::cleanup()
{
    QMutexLocker lock(&mutex_);
    
    if (swsContext_) {
        sws_freeContext(swsContext_);
        swsContext_ = nullptr;
    }
    
    if (rgbBuffer_) {
        av_free(rgbBuffer_);
        rgbBuffer_ = nullptr;
    }
    
    if (frameRGB_) {
        av_frame_free(&frameRGB_);
    }
    
    if (frame_) {
        av_frame_free(&frame_);
    }
    
    if (codecContext_) {
        avcodec_free_context(&codecContext_);
    }
    
    if (formatContext_) {
        avformat_close_input(&formatContext_);
    }
}

bool VideoThumbnailGeneratorFFmpeg::openFile(const QString& filePath)
{
    QMutexLocker lock(&mutex_);
    
    if (aborted_) {
        return false;
    }
    
    // Cleanup previous file if any
    cleanup();
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }
    
    // Open input file
    QByteArray pathBytes = filePath.toUtf8();
    int ret = avformat_open_input(&formatContext_, pathBytes.constData(), nullptr, nullptr);
    if (ret < 0) {
        return false;
    }
    
    // Find stream info
    ret = avformat_find_stream_info(formatContext_, nullptr);
    if (ret < 0) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Find video stream
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < formatContext_->nb_streams; i++) {
        if (formatContext_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }
    
    if (videoStreamIndex_ == -1) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Get codec parameters
    AVCodecParameters* codecpar = formatContext_->streams[videoStreamIndex_]->codecpar;
    
    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Allocate codec context
    codecContext_ = avcodec_alloc_context3(codec);
    if (!codecContext_) {
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Copy codec parameters to context
    ret = avcodec_parameters_to_context(codecContext_, codecpar);
    if (ret < 0) {
        avcodec_free_context(&codecContext_);
        codecContext_ = nullptr;
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Open codec
    ret = avcodec_open2(codecContext_, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&codecContext_);
        codecContext_ = nullptr;
        avformat_close_input(&formatContext_);
        formatContext_ = nullptr;
        return false;
    }
    
    // Allocate frames
    frame_ = av_frame_alloc();
    frameRGB_ = av_frame_alloc();
    if (!frame_ || !frameRGB_) {
        cleanup();
        return false;
    }
    
    // Allocate buffer for RGB frame
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext_->width, codecContext_->height, 1);
    rgbBuffer_ = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    if (!rgbBuffer_) {
        cleanup();
        return false;
    }
    av_image_fill_arrays(frameRGB_->data, frameRGB_->linesize, rgbBuffer_, AV_PIX_FMT_RGB24, 
                         codecContext_->width, codecContext_->height, 1);
    
    // Initialize SWS context for conversion
    swsContext_ = sws_getContext(
        codecContext_->width, codecContext_->height, codecContext_->pix_fmt,
        codecContext_->width, codecContext_->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!swsContext_) {
        cleanup();
        return false;
    }
    
    return true;
}

bool VideoThumbnailGeneratorFFmpeg::seekToPosition(int64_t timestamp)
{
    QMutexLocker lock(&mutex_);
    
    if (aborted_ || !formatContext_ || videoStreamIndex_ < 0) {
        return false;
    }
    
    // Seek to timestamp
    int64_t seekTarget = av_rescale_q(timestamp, AVRational{1, AV_TIME_BASE}, 
                                       formatContext_->streams[videoStreamIndex_]->time_base);
    
    int ret = av_seek_frame(formatContext_, videoStreamIndex_, seekTarget, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        return false;
    }
    
    // Flush codec buffers
    avcodec_flush_buffers(codecContext_);
    
    return true;
}

bool VideoThumbnailGeneratorFFmpeg::decodeFrame(AVFrame* frame)
{
    {
        QMutexLocker lock(&mutex_);
        if (aborted_ || !formatContext_ || !codecContext_ || !frame_) {
            return false;
        }
    }
    
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        return false;
    }
    
    while (true) {
        {
            QMutexLocker lock(&mutex_);
            if (aborted_ || !formatContext_) {
                av_packet_unref(packet);
                return false;
            }
        }
        
        int readResult = av_read_frame(formatContext_, packet);
        if (readResult < 0) {
            av_packet_unref(packet);
            break;
        }
        
        {
            QMutexLocker lock(&mutex_);
            if (aborted_ || !codecContext_) {
                av_packet_unref(packet);
                av_packet_free(&packet);
                return false;
            }
            
            if (packet->stream_index == videoStreamIndex_) {
                // Send packet to decoder
                int ret = avcodec_send_packet(codecContext_, packet);
                if (ret < 0) {
                    av_packet_unref(packet);
                    continue;
                }
                
                // Receive frame from decoder
                ret = avcodec_receive_frame(codecContext_, frame);
                if (ret == 0) {
                    av_packet_unref(packet);
                    av_packet_free(&packet);
                    return true;
                } else if (ret == AVERROR(EAGAIN)) {
                    // Need more packets
                    av_packet_unref(packet);
                    continue;
                } else {
                    // Error or end of stream
                    av_packet_unref(packet);
                    break;
                }
            }
        }
        
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    return false;
}

QImage VideoThumbnailGeneratorFFmpeg::frameToQImage(AVFrame* frame)
{
    if (!frame || !frameRGB_ || !swsContext_ || !codecContext_) {
        return QImage();
    }
    
    {
        QMutexLocker lock(&mutex_);
        if (aborted_) {
            return QImage();
        }
        
        // Convert frame to RGB
        sws_scale(swsContext_,
                  frame->data, frame->linesize, 0, codecContext_->height,
                  frameRGB_->data, frameRGB_->linesize);
        
        // Create QImage from RGB data (make a copy since frameRGB_ data is temporary)
        QImage image(frameRGB_->data[0], codecContext_->width, codecContext_->height,
                     frameRGB_->linesize[0], QImage::Format_RGB888);
        
        // QImage doesn't copy data, so we need to make a deep copy
        return image.copy();
    }
}

bool VideoThumbnailGeneratorFFmpeg::getThumbs(QString path, int count, QImage images[])
{
    {
        QMutexLocker lock(&mutex_);
        if (aborted_) {
            return false;
        }
    }
    
    // Initialize images array
    for (int i = 0; i < count; i++) {
        images[i] = QImage();
    }
    
    // Open file
    if (!openFile(path)) {
        return false;
    }
    
    // Get duration
    int64_t duration = formatContext_->duration;
    if (duration <= 0) {
        // Try to get duration from stream
        if (formatContext_->streams[videoStreamIndex_]->duration != AV_NOPTS_VALUE) {
            duration = av_rescale_q(formatContext_->streams[videoStreamIndex_]->duration,
                                   formatContext_->streams[videoStreamIndex_]->time_base,
                                   AVRational{1, AV_TIME_BASE});
        }
    }
    
    bool hasValidImage = false;
    
    // Generate thumbnails
    for (int i = 0; i < count; i++) {
        if (aborted_) {
            break;
        }
        
        // Calculate position (skip first 5% to avoid black frames)
        int64_t position = 0;
        if (duration > 0) {
            position = duration * (5 + (i + 1) * 90 / (count + 1)) / 100;
        }
        
        // Seek to position
        if (position > 0 && !seekToPosition(position)) {
            // If seek failed, try to get first frame
            if (!seekToPosition(0)) {
                continue;
            }
        }
        
        // Decode frame
        if (decodeFrame(frame_)) {
            QImage thumb = frameToQImage(frame_);
            if (!thumb.isNull()) {
                // Scale to thumbnail size (200x200)
                images[i] = thumb.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                hasValidImage = true;
            }
        } else {
            // If decode failed, try to get first frame
            if (position > 0) {
                if (seekToPosition(0) && decodeFrame(frame_)) {
                    QImage thumb = frameToQImage(frame_);
                    if (!thumb.isNull()) {
                        images[i] = thumb.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        hasValidImage = true;
                    }
                }
            }
        }
    }
    
    cleanup();
    
    return hasValidImage;
}

void VideoThumbnailGeneratorFFmpeg::abort()
{
    QMutexLocker lock(&mutex_);
    aborted_ = true;
}

#endif // HAVE_FFMPEG

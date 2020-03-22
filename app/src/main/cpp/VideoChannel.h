//
// Created by 王龙 on 2020-01-26.
//

#ifndef FFMPEGRTMP_VIDEOCHANNEL_H
#define FFMPEGRTMP_VIDEOCHANNEL_H


#include "BaseChannel.h"

#include "LogUtils.h"

extern "C" {
    #include <libswscale/swscale.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libavcodec/avcodec.h>
};

typedef void (*RenderCallback) (uint8_t * , int , int, int);

class VideoChannel : public BaseChannel {
public:
    VideoChannel(int streamIndex, AVCodecContext *pContext);
    ~VideoChannel();

    void start();

    void stop();

    void pause();

    void video_decode();

    void video_player();

    void setRenderCallback(RenderCallback renderCallback);

    void setTimeBase(AVRational base);
    void setReferenceClock(double * value);
private:
    pthread_t pid_video_decode;
    pthread_t pid_video_player;
    RenderCallback renderCallback;
    AVRational time_base;
    double *clock = 0;
};


#endif //FFMPEGRTMP_VIDEOCHANNEL_H

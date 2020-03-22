//
// Created by 王龙 on 2020-01-26.
//

#ifndef FFMPEGRTMP_AUDIOCHANNEL_H
#define FFMPEGRTMP_AUDIOCHANNEL_H


#include "BaseChannel.h"
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "LogUtils.h"


extern "C" {
    #include <libswscale/swscale.h>
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h> // 重采样支持
    #include <libavutil/mathematics.h>
};

class AudioChannel : public BaseChannel {
public:
    AudioChannel(int streamIndex, AVCodecContext *pContext);
    ~AudioChannel();

    void stop();

    void start();

    void pause();

    void audio_decode();

    void audio_player();

    int getPCM();

    void setTimeBase(AVRational base);
    void setReferenceClock(double * value);
    // 定义一个缓冲区
    uint8_t * out_buffers = 0;

    int out_channels;
    int out_sample_size;
    int out_sample_rate;
    int out_buffers_size;

private:
    // 线程ID
    pthread_t pid_audio_decode;
    pthread_t pid_audio_player;

    // 引擎
    SLObjectItf engineObject;
    // 引擎接口
    SLEngineItf engineInterface;
    // 混音器
    SLObjectItf outputMixObject;
    // 播放器的
    SLObjectItf bqPlayerObject;
    // 播放器接口
    SLPlayItf bqPlayerPlay;
    // 获取播放器队列接口
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;

    SwrContext *swr_ctx;
    AVRational time_base;
    double *clock = 0;
};


#endif //FFMPEGRTMP_AUDIOCHANNEL_H

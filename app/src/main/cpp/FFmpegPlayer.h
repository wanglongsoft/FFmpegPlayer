//
// Created by 王龙 on 2020-01-26.
//

#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H


#include <sys/types.h>
#include "AVCallback.h"
#include "AudioChannel.h"
#include "VideoChannel.h"
#include "LogUtils.h"

extern "C" {
    #include <libavformat/avformat.h>
};


class FFmpegPlayer {
public:
    FFmpegPlayer();

    FFmpegPlayer(const char *data_source, AVCallback *pCallback);

    ~FFmpegPlayer(); // 这个类没有子类，所以没有添加虚函数

    void prepare();

    void prepare_();

    void start();

    void start_();

    void setRenderCallback(RenderCallback callback);

    void pause();

    void stop();

private:
    char *data_source = 0;

    pthread_t pid_prepare ;

    AVFormatContext *formatContext = 0;

    AudioChannel *audioChannel = 0;
    VideoChannel *videoChannel = 0;
    AVCallback *pCallback;
    RenderCallback renderCallback;
    pthread_t pid_start;
    bool isPlaying;
    bool isPause;
    bool isStop;
    double referenceClock;
};


#endif //FFMPEGPLAYER_H

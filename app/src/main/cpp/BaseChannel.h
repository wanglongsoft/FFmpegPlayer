//
// Created by 王龙 on 2020-01-26.
//

#ifndef BASECHANNEL_H
#define BASECHANNEL_H

#include "safe_queue.h"
#include "LogUtils.h"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/time.h>
};

class BaseChannel {
public:
    int stream_index;

    BaseChannel(int stream_index, AVCodecContext *pContext) {
        this->stream_index = stream_index;
        this->pContext = pContext;
        packages.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
        isPause = 0;
        isStop = 0;
        isPlaying = 0;
    }

    // 注意：由于是父类，析构函数，必须是虚函数
    virtual ~BaseChannel() {
        LOGD("BaseChannel destruct");
        packages.clearQueue();
        frames.clearQueue();
        if(pContext) {
            avcodec_free_context(&pContext);
            pContext = nullptr;
        }
    }

    /**
     * 释放AVPacket 队列
     * @param avPacket
     */
    static void releaseAVPacket(AVPacket ** avPacket) {
        if (avPacket) {
            av_packet_free(avPacket);
            *avPacket = 0;
        }
    }

    /**
     * 释放AVFrame 队列
     * @param avFrame
     */
    static void releaseAVFrame(AVFrame ** avFrame) {
        if (avFrame) {
            av_frame_free(avFrame);
            *avFrame = 0;
        }
    }

    // AVPacket  音频：aac，  视频：h264
    SafeQueue<AVPacket *> packages; // 音频 或者 视频 的压缩数据包 (是编码的数据包)

    // AVFrame 音频：PCM，   视频：YUV
    SafeQueue<AVFrame *> frames; // 音频 或者 视频 的原始数据包（可以直接 渲染 和 播放 的）

    bool isPlaying = 0;
    bool isPause = 0;
    bool isStop = 0;

    AVCodecContext *pContext;
};


#endif //BASECHANNEL_H

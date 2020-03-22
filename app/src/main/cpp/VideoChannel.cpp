//
// Created by 王龙 on 2020-01-26.
//

#include "VideoChannel.h"

// 丢包的  函数指针 具体实现 frame
void dropAVFrame(std::queue<AVFrame *> & qq) {
    if (!qq.empty()) {
        AVFrame * frame = qq.front();
        BaseChannel::releaseAVFrame(&frame); // 释放掉
        qq.pop();
    }
}

// 丢包的  函数指针 具体实现 packet
void dropAVPacket(std::queue<AVPacket *> & qq) {
    if (!qq.empty()) {
        AVPacket* packet = qq.front();
        if (packet->flags != AV_PKT_FLAG_KEY) { // AV_PKT_FLAG_KEY == 关键帧
            BaseChannel::releaseAVPacket(&packet);
        }
        qq.pop();//直接pop掉，是否合适，容易内存泄漏(没有release)
    }
}

VideoChannel::VideoChannel(int streamIndex, AVCodecContext *pContext)
    : BaseChannel(streamIndex, pContext) {
    LOGD("width : %d   height : %d", pContext->width, pContext->height);
    this->packages.setSyncCallback(dropAVPacket);
    this->frames.setSyncCallback(dropAVFrame);
}

VideoChannel::~VideoChannel() {
    LOGD("VideoChannel destruct");
}

void * task_video_decode(void * pVoid) {
    VideoChannel * videoChannel = static_cast<VideoChannel *>(pVoid);
    videoChannel->video_decode();
    return 0;
}

void * task_video_player(void * pVoid) {
    VideoChannel * videoChannel = static_cast<VideoChannel *>(pVoid);
    videoChannel->video_player();
    return 0;
}

/**
 * 真正的解码，并且，播放
 * 1.解码（解码只有的是原始数据）
 * 2.播放
 */
void VideoChannel::start() {
    isPlaying = 1;
    isStop = 0;

    // 存放未解码的队列开始工作了
    packages.setFlag(1);

    // 存放解码后的队列开始工作了
    frames.setFlag(1);

    if(!isPause) {
        LOGD("new decode thread and play thread");
        // 1.解码的线程
        pthread_create(&pid_video_decode, 0, task_video_decode, this);

        // 2.播放的线程
        pthread_create(&pid_video_player, 0, task_video_player, this);
    } else {
        LOGD("not new decode thread and play thread");
        isPause = 0;
    }
}

void VideoChannel::stop() {
    LOGD("stop");
    isPlaying = 0;
    isPause = 0;
    isStop = 1;
}

/**
 * 运行在异步线程（视频真正解码函数）
 */
void VideoChannel::video_decode() {
    LOGD("video_decode start");
    AVPacket * packet = 0;
    while (isPlaying || isPause) {
        // 生产快  消费慢
        // 消费速度比生成速度慢（生成100，只消费10个，这样队列会爆）
        // 内存泄漏点2，解决方案：控制队列大小
        if (isPlaying && frames.queueSize() > 100) {
            // 休眠 等待队列中的数据被消费
            av_usleep(10 * 1000);
            continue;
        }

        int ret = packages.pop(packet);

        // 如果停止播放，跳出循环, 出了循环，就要释放
        if (!isPlaying && (!isPause)) {
            break;
        }

        if (!ret) {
            continue;
        }
        
        // 走到这里，就证明，取到了待解码的视频数据包
        ret = avcodec_send_packet(pContext, packet);
        if (ret) {
            if(ret == AVERROR(EAGAIN)) {
                LOGD("avcodec_send_packet fail : EAGAIN");
            } else if(ret == AVERROR_EOF) {
                LOGD("avcodec_send_packet fail : AVERROR_EOF");
            } else if(ret == AVERROR(EINVAL)) {
                LOGD("avcodec_send_packet fail : EINVAL");
            } else if(ret == AVERROR(ENOMEM)) {
                LOGD("avcodec_send_packet fail : ENOMEM");
            } else {
                LOGD("avcodec_send_packet fail : Other");
            }
            // 失败了（给”解码器上下文“发送Packet失败）
            LOGD("avcodec_send_packet fail : %d", ret);
            break;
        }

        // 走到这里，就证明，packet，用完毕了，成功了（给“解码器上下文”发送Packet成功），那么就可以释放packet了
        releaseAVPacket(&packet);

        AVFrame * frame = av_frame_alloc(); // AVFrame 拿到解码后的原始数据包
        ret = avcodec_receive_frame(pContext, frame);
        if (ret == AVERROR(EAGAIN)) {
            // 未取到关键帧，重来，重新取
            LOGD("not key frame , continue");
            releaseAVFrame(&frame);
            continue;
        } else if(ret != 0) {//decode error
            releaseAVFrame(&frame); // 内存释放点
            LOGD("avcodec_receive_frame fail");
            break;
        }

        // 终于取到了，解码后的视频数据（原始数据）
        frames.push(frame); // 加入队列
    }
    // 出了循环，就要释放
    releaseAVPacket(&packet);
    LOGD("VideoChannel video_decode end");
}

/**
 * 运行在异步线程（视频真正播放函数）
 */
void VideoChannel::video_player() {
    LOGD("VideoChannel video_player start");
    // 原始图形宽和高，可以通过解码器拿到
    // 1.TODO 把元素的 yuv  --->  rgba
    SwsContext * sws_ctx = sws_getContext(pContext->width, pContext->height, pContext->pix_fmt, // 原始图形信息相关的
                                          pContext->width, pContext->height, AV_PIX_FMT_RGBA, // 目标 尽量 要和 元素的保持一致
                                          SWS_BILINEAR, NULL, NULL, NULL
    );

    // 2.TODO 给dst_data rgba 这种 申请内存
    uint8_t * dst_data[4];//rgba == 4
    int dst_linesize[4];//rgba == 4
    AVFrame * frame = 0;

    av_image_alloc(dst_data, dst_linesize, pContext->width, pContext->height, AV_PIX_FMT_RGBA, 1);
    // 3.TODO 原始数据 格式转换的函数 （从队列中，拿到（原始）数据，一帧一帧的转换（rgba），一帧一帧的渲染到屏幕上）

    double video_delay = 0;

    while(isPlaying || isPause) {

        if(isPause) {
            av_usleep(20 * 1000);
            continue;
        }

        int ret = frames.pop(frame);

        // 如果停止播放，跳出循环, 出了循环，就要释放

        if (!ret) {
            continue;
        }

        if (!isPlaying && !isPause) {
            break;
        }

        // 格式的转换 (yuv --> rgba)   frame->data == yuv是原始数据      dst_data是rgba格式的数据
        sws_scale(sws_ctx, frame->data,
                  frame->linesize, 0, pContext->height, dst_data, dst_linesize);

        // 渲染，显示在屏幕上了
        // 渲染的两种方式：
        // 渲染一帧图像（宽，高，数据）
        renderCallback(dst_data[0], pContext->width, pContext->height , dst_linesize[0]);

        if(*clock > 0 && (frame->pts * av_q2d(time_base) > 0)) { //简易音视频同步
            video_delay = frame->pts * av_q2d(time_base) - *clock;
            if(video_delay > 0) {
                if(video_delay > 1) {
                    av_usleep(video_delay * 1000000 * 2);
                } else {
                    av_usleep(video_delay * 1000000);
                }
            } else if(video_delay < 0) {
                releaseAVFrame(&frame);
                frames.syncAction();
                continue;
            } else {

            }
        }
        releaseAVFrame(&frame); // 渲染完了，frame没用了，释放掉
    }
    releaseAVFrame(&frame);
    isPlaying = 0;
    av_freep(&dst_data[0]);
    av_freep(&dst_linesize);
    sws_freeContext(sws_ctx);
}

void VideoChannel::setRenderCallback(RenderCallback renderCallback) {
    this->renderCallback = renderCallback;
}

void VideoChannel::setTimeBase(AVRational base) {
    this->time_base = base;
}

void VideoChannel::setReferenceClock(double *value) {
    clock = value;
}

void VideoChannel::pause() {
    isPlaying = 0;
    isStop = 0;
    isPause = 1;
}

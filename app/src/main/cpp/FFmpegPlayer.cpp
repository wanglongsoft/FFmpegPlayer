//
// Created by 王龙 on 2020-01-26.
//

#include <pthread.h>
#include "FFmpegPlayer.h"

void * customTaskPrepareThread(void * pVoid) {
    FFmpegPlayer * fFmpegPlayer = static_cast<FFmpegPlayer *>(pVoid);
    fFmpegPlayer->prepare_();
    return 0; // 坑：一定要记得return
}

// TODO 异步 函数指针 - 开始播放工作start
void * customTaskStartThread(void * pVoid) {
    FFmpegPlayer * fFmpegPlayer = static_cast<FFmpegPlayer *>(pVoid);
    fFmpegPlayer->start_();
    return 0; // 坑：一定要记得return
}

FFmpegPlayer::FFmpegPlayer() {
}

FFmpegPlayer::FFmpegPlayer(const char *data_source, AVCallback *pCallback) {
    this->data_source = new char[strlen(data_source) + 1];
    strcpy(this->data_source, data_source);
    this->pCallback = pCallback;
    referenceClock = 0;
    isPause = 0;
    isStop = 0;
    isPlaying = 0;
}

FFmpegPlayer::~FFmpegPlayer() {
    LOGD("FFmpegPlayer destruct");
    if (this->data_source) {
        delete this->data_source;
        this->data_source = 0;
    }
    if(audioChannel) {
        delete audioChannel;
    }
    if(videoChannel) {
        delete videoChannel;
    }

    if(formatContext) {
        avformat_close_input(&formatContext);//关闭输入流
        avformat_free_context(formatContext);
        formatContext = nullptr;
    }

}

void FFmpegPlayer::prepare() {
    pthread_create(&pid_prepare, 0, customTaskPrepareThread, this);
}

void FFmpegPlayer::prepare_() {
    char errors[1024];
    formatContext = avformat_alloc_context();
    AVDictionary *dictionary = nullptr;
    av_dict_set(&dictionary, "timeout", "5000000", 0);
    int ret = avformat_open_input(&formatContext, data_source, 0, 0);
    av_dict_free(&dictionary);
    LOGD("avformat_open_input ret == %d", ret);
    if(ret < 0) {
        av_strerror(ret, errors, 1024);
        LOGD("Could not open source file: %s, %d(%s)", data_source,
             ret,
             errors);
        this->pCallback->onError(100, THREAD_SUB);
        return;
    }
    ret = avformat_find_stream_info(formatContext, 0);
    LOGD("avformat_find_stream_info ret == %d", ret);
    if(ret < 0) {
        av_strerror(ret, errors, 1024);
        LOGD("avformat_find_stream_info fail: %d(%s)", ret, errors);
        this->pCallback->onError(101, THREAD_SUB);
        return;
    }
    
    for (int stream_index = 0; stream_index < formatContext->nb_streams; ++stream_index) {
        AVStream * stream = formatContext->streams[stream_index];
        AVCodecParameters * codecParameters = stream->codecpar;
        AVCodec * codec = avcodec_find_decoder(codecParameters->codec_id);
        if(!codec) {
            LOGD("avcodec_find_decoder find fail");
            this->pCallback->onError(102, THREAD_SUB);
            return;
        }
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if(!codecContext) {
            LOGD("avcodec_alloc_context find fail");
            this->pCallback->onError(103, THREAD_SUB);
            return;
        }
        ret = avcodec_parameters_to_context(codecContext, codecParameters);
        LOGD("avcodec_parameters_to_context ret == %d", ret);
        if(ret < 0) {
            LOGD("avcodec_parameters_to_context fail");
            this->pCallback->onError(104, THREAD_SUB);
            return;
        }
        ret =  avcodec_open2(codecContext, codec, 0);
        LOGD("avcodec_open2 ret == %d", ret);
        if(ret < 0) {
            LOGD("avcodec_open2 fail");
            this->pCallback->onError(105, THREAD_SUB);
            return;
        }
        LOGD("codecParameters->codec_type == %d", codecParameters->codec_type);
        if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO) { // 音频流
            audioChannel = new AudioChannel(stream_index, codecContext);
            audioChannel->setTimeBase(stream->time_base);
            audioChannel->setReferenceClock(&referenceClock);
        } else if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO) { // 视频流
            videoChannel = new VideoChannel(stream_index, codecContext);
            videoChannel->setRenderCallback(renderCallback);
            videoChannel->setTimeBase(stream->time_base);
            videoChannel->setReferenceClock(&referenceClock);
        }
    }

    if (audioChannel || videoChannel) {
        LOGD("prepare success");
        this->pCallback->onPrepare(THREAD_SUB);
    } else {
        this->pCallback->onError(106, THREAD_SUB);
        LOGD("prepare fail");
    }
}

void FFmpegPlayer::start() {
    LOGD("FFmpegPlayer start");
    isPlaying = 1;
    isStop = 0;

    if (videoChannel) {
        videoChannel->start();
    }

    if (audioChannel) {
        audioChannel->start();
    }

    if(!isPause) {
        LOGD("new decompression thread");
        pthread_create(&pid_start, 0, customTaskStartThread, this);
    } else {
        LOGD("not new decompression thread");
        isPause = 0;
    }
}

void FFmpegPlayer::start_() {
    LOGD("FFmpegPlayer get packet start");
    while (isPlaying || isPause) {
        // 内存泄漏点1，解决方案：控制队列大小
        if (videoChannel && videoChannel->packages.queueSize() > 100) {
            // 休眠 等待队列中的数据被消费
            av_usleep(10 * 1000);
            continue;
        }
        // 内存泄漏点1，解决方案：控制队列大小
        if (audioChannel && audioChannel->packages.queueSize() > 100) {
            // 休眠 等待队列中的数据被消费
            av_usleep(10 * 1000);
            continue;
        }

        if(isPause) {
            av_usleep(20 * 1000);
            continue;
        }

        // AVPacket 可能是音频 可能是视频, 没有解码的（数据包）
        AVPacket * packet = av_packet_alloc();
        int ret = av_read_frame(formatContext, packet); // 这行代码一执行完毕，packet就有（音视频数据）
        if (!ret) { // ret == 0
            // 把已经得到的packet 放入队列中
            // 先判断是视频  还是  音频， 分别对应的放入 音频队列  视频队列

            // packet->stream_index 对应之前的prepare中循环的i

            if (videoChannel && videoChannel->stream_index == packet->stream_index) {
                // 如果他们两 相等 说明是视频  视频包

                // 未解码的 视频数据包 加入到队列
                videoChannel->packages.push(packet);
            } else if (audioChannel && audioChannel->stream_index == packet->stream_index) {
                // 如果他们两 相等 说明是音频  音频包

                // 未解码的 音频数据包 加入到队列
                audioChannel->packages.push(packet);
            } else {
                LOGD("av_packet_free not AV packet");
                av_packet_free(&packet);
            }
        } else if (ret == AVERROR_EOF) {  // or   end of file， 文件结尾，读完了 的意思
            // 代表读完了
            // TODO 一定是要 读完了 并且 也播完了，才做事情
            LOGD("AVPacket get complete");
            av_packet_free(&packet);
            break;
        } else {
            // 代表失败了，有问题
            av_packet_free(&packet);
            LOGD("AVPacket av_read_frame fail");
            break;
        }
    }
    // end while
    isPlaying = 0;
    LOGD("FFmpegPlayer get packet end");
}

void FFmpegPlayer::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}

void FFmpegPlayer::stop() {
    isPlaying = 0;
    isPause = 0;
    isStop = 1;
    if(videoChannel) {
        videoChannel->stop();
    }

    if(audioChannel) {
        audioChannel->stop();
    }
}

void FFmpegPlayer::pause() {
    isPlaying = 0;
    isStop = 0;
    isPause = 1;
    videoChannel->pause();
    audioChannel->pause();
}

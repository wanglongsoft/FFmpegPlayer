//
// Created by 王龙 on 2020-01-26.
//

#include "AudioChannel.h"

AudioChannel::AudioChannel(int streamIndex, AVCodecContext *pContext)
    : BaseChannel(streamIndex, pContext) {
    // 初始化 缓冲区 out_buffers
    // 如何去定义缓冲区？
    // 答：根据数据类型 44100  和  16bits  和  2声道

    // 可以是写死的方式
    // out_buffers_size = 44100 * 2 * 2;

    // 当然也可以是 动态获得，如何计算
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    // 通道数AV_CH_LAYOUT_STEREO(双声道的意思)
    out_sample_size = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sample_rate = 44100; // 采样率
    out_buffers_size = out_sample_rate * out_sample_size * out_channels;

    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers, 0, out_buffers_size);

    // swr_ctx = swr_alloc();
    swr_ctx = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, out_sample_rate,
                                 pContext->channel_layout, pContext->sample_fmt, pContext->sample_rate, 0 ,0);

    //防止有吱吱声音
    swr_init(swr_ctx);
}

AudioChannel::~AudioChannel() {
    // TODO swr_ctx 要释放
    LOGD("AudioChannel destruct");
    free(out_buffers);
    swr_free(&swr_ctx);
}

void * task_audio_decode(void * pVoid) {
    AudioChannel * audioChannel = static_cast<AudioChannel *>(pVoid);
    audioChannel->audio_decode();
    return 0;
}

void * task_audio_player(void * pVoid) {
    AudioChannel * audioChannel = static_cast<AudioChannel *>(pVoid);
    audioChannel->audio_player();
    return 0;
}

/**
 * 1.解码
 * 2.播放
 */
void AudioChannel::start() {
    isPlaying = 1;
    isStop = 0;

    // 存放未解码的队列开始工作了
    packages.setFlag(1);

    // 存放解码后的队列开始工作了
    frames.setFlag(1);

    if(!isPause) {
        LOGD("new decode thread and play thread");
        // 1.解码的线程
        pthread_create(&pid_audio_decode, 0, task_audio_decode, this);

        // 2.播放的线程
        pthread_create(&pid_audio_player, 0, task_audio_player, this);
    } else {
        LOGD("not new decode thread and play thread");
        isPause = 0;
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    }
}

void AudioChannel::stop() {
    isPlaying = 0;
    isStop = 1;
    isPause = 0;

    // TODO 第七大步：释放操作
    // 7.1 设置停止状态
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
        bqPlayerPlay = 0;
    }
    // 7.2 销毁播放器
    if (bqPlayerObject) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    // 7.3 销毁混音器
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    // 7.4 销毁引擎
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }
    LOGD("stop");
    isPlaying = 0;
}

/**
 * 音频解码（运行在子线程）
 */
void AudioChannel::audio_decode() {
    LOGD("audio_decode start");
    AVPacket * packet = 0;
    while (isPlaying || isPause) {
        // 生产快  消费慢
        // 消费速度比生成速度慢（生成100，只消费10个，这样队列会爆）
        // 内存泄漏点1，解决方案：控制队列大小
        if (isPlaying && frames.queueSize() > 100) {
            // 休眠 等待队列中的数据被消费
            av_usleep(10 * 1000);
            continue;
        }

        int ret = packages.pop(packet);

        // 如果停止播放，跳出循环, 出了循环，就要释放
        if (!isPlaying && !isPause) {
            break;
        }

        if (!ret) {
            continue;
        }

        // 走到这里，就证明，取到了待解码的音频数据包
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
            // 重来，重新取
            continue;
        } else if(ret != 0) {
            releaseAVFrame(&frame); // 内存释放点
            LOGD("avcodec_receive_frame fail");
            break;
        }

        // 终于取到了，解码后的音频数据（原始数据）
        frames.push(frame); // 加入队列
    }
    // 出了循环，就要释放
    releaseAVPacket(&packet);
    LOGD("audio_decode end");
}

// 4.3 创建回调函数
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    // PCM 在 frames 队列中，所以和视频一样，也要这些操作才行
    AudioChannel * audioChannel = static_cast<AudioChannel *>(context);
    int pcmSize = audioChannel->getPCM();

    (*bq)->Enqueue(bq, audioChannel->out_buffers , pcmSize);
}

/**
 * 获取PCM数据
 * @return  PCM数据大小
 */
int AudioChannel::getPCM() {
    int pcm_data_size = 0;

    // PCM 在 frames 队列中
    AVFrame * frame = 0;
    while(isPlaying) {
        int ret = frames.pop(frame);

        // 如果停止播放，跳出循环, 出了循环，就要释放
        if (!isPlaying) {
            break;
        }

        if (!ret) {
            continue;
        }

        // 音频播放器的数据格式是我们在下面定义的（16位 双声道 ....）
        // 而原始数据（是待播放的音频PCM数据）
        // 所以，上面的两句话，无法统一，一个是(自己定义的16位 双声道 ..) 一个是原始数据，为了解决上面的问题，就需要重采样。
        // 开始重采样
        int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) +
                                            frame->nb_samples, out_sample_rate, frame->sample_rate, AV_ROUND_UP);

        ret = swr_convert(swr_ctx, &out_buffers, dst_nb_samples, (const uint8_t **) frame->data, frame->nb_samples);
        if (ret < 0) {
            LOGD("swr_convert fail");
        }

        // pcm_data_size = ret * out_sample_size; // 每个声道的数据
        pcm_data_size = ret * out_sample_size * out_channels;

        if(nullptr != clock) {//音视频同步，参考时钟为：音频
            if (frame->pts != AV_NOPTS_VALUE) {
                *clock = av_q2d(time_base) * frame->pts;
            }
        }

        break;
    } // end while
    releaseAVFrame(&frame); // 渲染完了，frame没用了，释放掉
    return pcm_data_size;
}

/**
 * 音频播放（运行在子线程）
 */
void AudioChannel::audio_player() {
    LOGD("audio_player start");
    // 音频的播放，就涉及到了，OpenLSES

    // TODO 第一大步：创建引擎并获取引擎接口
    // 1.1创建引擎对象：SLObjectItf engineObject
    SLresult result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // 1.2 初始化引擎
    result = (*engineObject) ->Realize(engineObject, SL_BOOLEAN_FALSE);
    if (SL_BOOLEAN_FALSE != result) {//创建失败
        return;
    }

    // 1.3 获取引擎接口 SLEngineItf engineInterface
    result = (*engineObject) ->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // TODO 第二大步 设置混音器
    // 2.1 创建混音器：SLObjectItf outputMixObject
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // 2.2 初始化 混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    if (SL_BOOLEAN_FALSE != result) {
        return;
    }
    //  不启用混响可以不用获取混音器接口
    //  获得混音器接口
    //  result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
    //                                         &outputMixEnvironmentalReverb);
    //  if (SL_RESULT_SUCCESS == result) {
    //  设置混响 ： 默认。
    //  SL_I3DL2_ENVIRONMENT_PRESET_ROOM: 室内
    //  SL_I3DL2_ENVIRONMENT_PRESET_AUDITORIUM : 礼堂 等
    //  const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    //  (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
    //       outputMixEnvironmentalReverb, &settings);
    //  }

    // TODO 第三大步 创建播放器
    // 3.1 配置输入声音信息
    // 创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                       2};
    // pcm数据格式
    // SL_DATAFORMAT_PCM：数据格式为pcm格式
    // 2：双声道
    // SL_SAMPLINGRATE_44_1：采样率为44100（44.1赫兹 应用最广的，兼容性最好的）
    // SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit （16位）(2个字节)
    // SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit （16位）（2个字节）
    // SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）  （双声道 立体声的效果）
    // SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM,
                                   2,
                                   SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};

    // 数据源 将上述配置信息放到这个数据源中
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // 3.2 配置音轨（输出）
    // 设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    //  需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    //  3.3 创建播放器
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc, &audioSnk, 1, ids, req);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //  3.4 初始化播放器：SLObjectItf bqPlayerObject
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }
    //  3.5 获取播放器接口：SLPlayItf bqPlayerPlay
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    // TODO 第四大步：设置播放回调函数
    // 4.1 获取播放器队列接口：SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);

    // 4.2 设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    // TODO 第五大步：设置播放器状态为播放状态
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    // TODO 第六步：手动激活回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);
    LOGD("audio_player end");
}

void AudioChannel::setTimeBase(AVRational base) {
    this->time_base = base;
}

void AudioChannel::setReferenceClock(double *value) {
    clock = value;
}

void AudioChannel::pause() {
    isPlaying = 0;
    isStop = 0;
    isPause = 1;
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
}

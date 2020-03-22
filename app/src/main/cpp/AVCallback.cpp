//
// Created by 王龙 on 2020-01-26.
//

#include "AVCallback.h"


AVCallback::AVCallback(JavaVM *pVm, JNIEnv *pEnv, jobject instance) {
    this->jvm = pVm;
    this->jenv = pEnv;
    this->instance = pEnv->NewGlobalRef(instance); // 需要是全局（jobject一旦涉及到跨函数，跨线程，必须是全局引用）
    jclass mFFmpegPlayerClass = jenv->GetObjectClass(this->instance);
    this->methodId_onPrepare = this->jenv->GetMethodID(mFFmpegPlayerClass, "onPrepared", "()V");
    this->methodId_onError = this->jenv->GetMethodID(mFFmpegPlayerClass, "onError", "(I)V");
}

AVCallback::~AVCallback() {
    this->jvm = 0;
    jenv->DeleteGlobalRef(this->instance);
    this->instance = 0;
    jenv = 0;
}

void AVCallback::onError(int error_code, int thread_mode) {
    LOGD("onError : %d", error_code);
    if(THREAD_MAIN == thread_mode) {
        this->jenv->CallVoidMethod(this->instance, this->methodId_onError, error_code);
    } if(THREAD_SUB == thread_mode) {
        JNIEnv *local_env = nullptr;
        jint ret = this->jvm->AttachCurrentThread(&local_env, nullptr);
        if (ret != JNI_OK) {
            LOGD("onPrepare AttachCurrentThread error , return");
            return;
        }
        local_env->CallVoidMethod(this->instance, this->methodId_onError, error_code);
        this->jvm->DetachCurrentThread();
    } else {
        LOGD("onPrepare thread unknown");
    }
}

void AVCallback::onPrepare(int thread_mode) {
    LOGD("onPrepare");
    if(THREAD_MAIN == thread_mode) {
        this->jenv->CallVoidMethod(this->instance, this->methodId_onPrepare);
    } if(THREAD_SUB == thread_mode) {
        JNIEnv *local_env = nullptr;
        jint ret = this->jvm->AttachCurrentThread(&local_env, nullptr);
        if (ret != JNI_OK) {
            LOGD("onPrepare AttachCurrentThread error , return");
            return;
        }
        local_env->CallVoidMethod(this->instance, this->methodId_onPrepare);
        this->jvm->DetachCurrentThread();
    } else {
        LOGD("onPrepare thread unknown");
    }
}

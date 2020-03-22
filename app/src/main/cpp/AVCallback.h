//
// Created by 王龙 on 2020-01-26.
//

#ifndef AVCALLBACK_H
#define AVCALLBACK_H


#include <jni.h>

#include "LogUtils.h"
#include "CommonDefine.h"

class AVCallback {
public:
    AVCallback(JavaVM *pVm, JNIEnv *pEnv, jobject instance);
    ~AVCallback();
    void onError(int error_code, int thread_mode);
    void onPrepare(int thread_mode);
private:
    JavaVM * jvm;
    JNIEnv * jenv;
    jobject instance;
    jmethodID methodId_onError;
    jmethodID methodId_onPrepare;
};


#endif //AVCALLBACK_H

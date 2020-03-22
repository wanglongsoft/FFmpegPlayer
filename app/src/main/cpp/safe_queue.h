//
// Created by Administrator on 2020/1/5.
//

#ifndef SAFE_QUEUE_H
#define SAFE_QUEUE_H

#include <queue>
#include <pthread.h>

template <typename T>

class SafeQueue
{
    // Java的回调   ===  C语言的函数指针
    typedef void (*ReleaseCallback) (T *);
    typedef void (*SyncCallback) (std::queue<T> &);

public:
    SafeQueue()
    {
        pthread_mutex_init(&mutex, 0);
        pthread_cond_init(&cond, 0);
    }

    ~SafeQueue()
    {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    /**
     * 入队
     */
     void push(T value) {
        pthread_mutex_lock(&mutex); // 为了线程的安全性，锁上

        if (flag) {
            q.push(value);
            pthread_cond_signal(&cond); // 通知
        } else {
            // 释放操作（不知道value是什么类型，int， 对象 ，等等，怎么办？）
            // 我们不知道，交给用户来处理
            if (releaseCallback) {
                releaseCallback(&value);
            }
        }

        pthread_mutex_unlock(&mutex); // 为了让其他线程可以进来，解锁
     }

    /**
     * 出队
     */
    int pop(T & t) {
        int ret = 0;

        pthread_mutex_lock(&mutex); // 为了线程的安全性，锁上

        while (flag && q.empty()) {
            // 如果工作状态 并且 队列中没有数据，就等待）（排队）
            pthread_cond_wait(&cond, &mutex);
        }

        if (!q.empty()) {
            t = q.front();
            q.pop();
            ret = 1;
        }

        pthread_mutex_unlock(&mutex); // 为了让其他线程可以进来，解锁

        return ret;
     }

     void setFlag(int flag) {
         pthread_mutex_lock(&mutex); // 为了线程的安全性，锁上

         this->flag = flag;
         pthread_cond_signal(&cond); // 通知

         pthread_mutex_unlock(&mutex); // 为了让其他线程可以进来，解锁
    }

    int isEmpty() {
        return q.empty();
    }

    int queueSize() {
        return q.size();
    }

    void clearQueue() {
        pthread_mutex_lock(&mutex); // 为了线程的安全性，锁上

        unsigned int  size = q.size();
        for (int i = 0; i < size; ++i) {
            // 循环 一个个的释放
            T value = q.front();
            if (releaseCallback) {
                releaseCallback(&value);
            }
            q.pop();
        }

        pthread_mutex_unlock(&mutex); // 为了让其他线程可以进来，解锁
    }

    void setReleaseCallback(ReleaseCallback releaseCallback) {
        this->releaseCallback = releaseCallback;
    }

    void setSyncCallback(SyncCallback syncCallback) {
        this->syncCallback = syncCallback;
    }

    /**
     * 同步 丢帧 操作
     */
    void syncAction() {
        pthread_mutex_lock(&mutex); // 为了线程的安全性，锁上

        // 我这里面不做任何逻辑，丢给外面用户去完成(谁实现，谁就去完成具体逻辑)
        syncCallback(q);

        pthread_mutex_unlock(&mutex); // 为了让其他线程可以进来，解锁
    }

private:
    std::queue<T> q;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int flag; // 标记队列释放工作[true=工作状态，false=非工作状态]
    ReleaseCallback releaseCallback;
    SyncCallback syncCallback;
};

#endif //KEVINPLAYER_SAFE_QUEUE_H

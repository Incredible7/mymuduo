#pragma once

#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread 
{
    // 保存tid缓存，因为系统调用非常耗时，拿到tid后将其缓存
    //__thread是GCC内置的线程局部存储措施，每一个线程有一份__thread独立实体，各个线程值互不干扰
    // 有且只有thread_local关键字修饰的变量具有线程（thread）周期，这些变量在线程开始的时候被生成，
    // 在线程结束的时候被销毁，并且每一个线程都拥有一个独立的变量实例。

    extern thread_local int t_cacheadTid;

    void cacheTid();

    inline int tid() {
        // 此语句意思是如果还未获取tid，进入if，通过cacheTid()系统调用获取tid
        if (t_cacheadTid == 0) {
            cacheTid();
        }
        return t_cacheadTid;
    }
};  // namespace CurrentThread
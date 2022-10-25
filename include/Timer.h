#pragma once
#include <atomic>

#include "Callbacks.h"
#include "Timestamp.h"
#include "noncopyable.h"

// 为管理时间事件设计的类
class Timer : noncopyable {
public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(s_numCreated++) {}

    void run() const {
        callback_();
    }

    Timestamp expiration() const {
        return expiration_;
    }
    bool repeat() const {
        return repeat_;
    }
    int64_t sequence() const {
        return sequence_;
    }

    static int64_t numCreated() {
        return s_numCreated;
    }
    void restart(Timestamp now);

private:
    // 时间事件回调函数
    const TimerCallback callback_;
    // 超时时间
    Timestamp expiration_;
    // 定时事件间隔
    const double interval_;
    const bool   repeat_;
    // 事件事件序号
    const int64_t sequence_ = 0;
    // 共有多少个时间事件
    static std::atomic_int_least64_t s_numCreated;
};
#pragma once

#include "Callbacks.h"
#include "Channel.h"
#include "Timestamp.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

class EventLoop;
class Timer;
class TimerId;

// 不保证回调准时进行
class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 经常被其他的线程调用，必须是线程安全的
    // 添加定时事件
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timetrId);

    void handleRead();

    // 移除失效timer
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool       insert(Timer* timer);
    EventLoop* loop_;

    const int timerfd_;
    Channel   timerfdChannel_;

    // 按过期时间排序的计时列表
    TimerList timers_;

    // for cancel()
    ActiveTimerSet   activeTimers_;
    std::atomic_bool callingExpiredTimers_;
    ActiveTimerSet   cancelingTimers_;
};
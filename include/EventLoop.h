#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "Callbacks.h"
#include "CurrentThread.h"
#include "TimerId.h"
#include "TimerQueue.h"
#include "Timestamp.h"
#include "noncopyable.h"

class Channel;
class Poller;

// 事件循环类，主要包含两大模块，Channel Poller
// 创建了EventLoop的线程就是IO线程，主要功能是EventLoop::loop()
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const {
        return pollReturnTime_;
    }

    // 立即唤醒当前loop所在线程并在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把回调函数放入队列，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    // 在time时间点执行回调函数
    TimerId runAt(Timestamp time, TimerCallback cb);
    // 在delay时间后执行回调函数
    TimerId runAfter(double delay, TimerCallback cb);
    // 每隔interval秒执行一次回调函数
    TimerId runEvery(double interval, TimerCallback cb);

    // 取消时间事件
    void cancel(TimerId timerId);
    // 通过eventfd唤醒loop所在线程
    void wakeup();

    // EentLoop的方法  => Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // 判断EventLoop对象是否在自己的线程里
    // threadId_为EventLoop创建时的线程id，CurrentThread::tid()为当前线程id
    bool isInLoopThread() const {
        return threadId_ == CurrentThread::tid();
    }

    // 每个线程只能有一个EventLoop对象，如果该线程已经有其他的EventLoop对象，退出程序
    void assertInLoopThread() {
        if (!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }

private:
    // 给eventfd返回的文件描述符wakeupFd_绑定的事件回调，当wakeup()时，即有事件发生时
    // 调用handleRead()读取wakeupFd_的 8字节，同时唤醒阻塞的epoll_wait
    void handleRead();
    // 执行上层的回调函数
    void doPendingFunctors();
    // 退出不在循环中的线程
    void abortNotInLoopThread();

    using ChannelList = std::vector<Channel*>;
    // 原子操作，底层通过CAS实现。CAS（Compare-And-Swap）,是一条CPU并发原语，
    // 用于判断内存中某个位置的值是否为预期值，如果是则更改为新的值，这个过程是原子的。
    std::atomic_bool looping_;
    std::atomic_bool quit_;

    const pid_t threadId_;  // 记录当前EventLoop 是被哪一个线程id创建的，

    Timestamp pollReturnTime_;  // Poller返回时间的Channels的时间点
    std::unique_ptr<Poller> poller_;

    // mainLoop获取一个新用户的Channel需要通过轮询算法选择一个subLoop，通过该成员唤醒subLoop处理Channel
    int                      wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    // 返回Poller检测到的当前有事件发生的所有Channel列表
    ChannelList activeChannels_;

    // 定时事件管理器
    std::unique_ptr<TimerQueue> timerQueue_;

    // 标识当前loop是否有需要执行的回调函数
    std::atomic_bool callingPendingFunctors_;

    // 存储loop需要执行的所有回调操作
    std::vector<Functor> pendingFunctors_;

    // 互斥锁，用来保护上面vector的线程安全
    std::mutex mutex_;
};
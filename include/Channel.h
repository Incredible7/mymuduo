#pragma once

#include "Timestamp.h"
#include "noncopyable.h"

#include <functional>
#include <memory>

class EventLoop;

// Channel为通道，封装了sockfd感兴趣的event，
// 如EPOLLIN，EPOLLOUT事件，还绑定了poller返回的具体事件
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    // fd得到Poller通知后，处理事件handleEvent在EventLoop::loop()中调用
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象，由TcpConnection设置
    void setReadCallback(ReadEventCallback cb) {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb) {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb) {
        errorCallback_ = std::move(cb);
    }

    // 防止当channel执行回调时被手动remove掉
    void tie(const std::shared_ptr<void>&);

    int fd() const {
        return fd_;
    }
    int events() const {
        return events_;
    }
    void set_revents(int revt) {
        revents_ = revt;
    }

    // 设置fd相应的事件状态，相当于epoll_ctl add delete...
    void disableReading() {
        events_ &= ~kReadEvent;
        update();
    }
    void enableReading() {
        events_ |= kReadEvent;
        update();
    }
    void disableWriting() {
        events_ &= ~kWriteEvent;
        update();
    }
    void enableWriting() {
        events_ |= kWriteEvent;
        update();
    }
    void disableAll() {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前状态
    bool isNoneEvent() const {
        return events_ == kNoneEvent;
    }
    bool isWriting() const {
        return events_ == kWriteEvent;
    }
    bool isReading() const {
        return events_ == kReadEvent;
    }

    int index() {
        return index_;
    }
    void set_index(int idx) {
        index_ = idx;
    }

    //每个线程的循环
    EventLoop* ownerLoop() {
        return loop_;
    }

    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    // 设置（更新）fd的事件状态
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;     // 事件循环
    const int  fd_;       // fd Poller监听的对象
    int        events_;   // 注册fd感兴趣的事件
    int        revents_;  // Poller返回的发生的具体事件
    int        index_;  // 用于标记该channel对象在Poller中的状态

    std::weak_ptr<void> tie_;
    bool                tied_;

    // Channel通道知道fd最终发生的具体事件events，
    // 因此它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback     writeCallback_;
    EventCallback     closeCallback_;
    EventCallback     errorCallback_;
};
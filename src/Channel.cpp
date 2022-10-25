#include <sys/epoll.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent =
    EPOLLIN | EPOLLPRI;  // 连接建立；有数据到达；带外数据
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      tied_(false) {}

// TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection对象成员的方法，
// 因此可以说明：Channel的结束一定早于TcpConnection对象。
// 此处tie去解决TcpConnection和Channel的生命周期时长问题，从而保证Channel对象能够在TcpConnection销毁前销毁
void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

// 当改变channel所表示的fd的events事件后，update负责在poller里面更改fd相应的事件（epoll_ctl）
void Channel::update() {
    // 通过channel所属的eventloop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中把当前的channel删除
void Channel::remove() {
    loop_->removeChannel(this);
}

// Channel的核心
void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
        // 如果提升失败，说明Channel的TcpConnection对象已经不存在，不做处理
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("channel handleEvent revents:%d", revents_);
    // 当TcpConnection对应的Channel通过shutdown关闭写端，epoll触发EPOLLHUP
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) {
            closeCallback_();
        }
    }
    // 错误
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    // 读
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }
    // 写
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}
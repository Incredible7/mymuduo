#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "Channel.h"
#include "EPollPoller.h"
#include "Logger.h"

// 某个channel还没有添加到Poller     channel的index_初始化为-1
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

// epoll_create()源码中直接调用了epoll_create1()
// EPOLL_CLOEXEC表示在fork后关闭子进程中无用文件描述符,即fork创建的子进程在子进程中关闭该socket
// 其实不加也行，因为内核已经自动加上这个选项了
EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error:%d", errno);
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    // poll频繁调用，用DEBUG比较合适
    LOG_DEBUG("func=%s => fd total count:%1u", __FUNCTION__,
              channels_.size());

    int numEvents =
        ::epoll_wait(epollfd_, &*events_.begin(),
                     static_cast<int>(events_.size()), timeoutMs);
    int       saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_DEBUG("%d events happened", numEvents);  // LOG_DEBUG最合理
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) {  // 扩容
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout!", __FUNCTION__);
    } else {  // 发生错误
        if (saveErrno != EINTR /* Interrupted system call */) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// Channel update|remove => EventLoop updateChannel...
// => Poller updateChannel
void EPollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();
    LOG_DEBUG("func=%s => fd=%d events=%d index=%d", __FUNCTION__,
              channel->fd(), channel->events(), index);

    // channel是新来的或者已经删除过的
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        } else {
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {  // 已经在Poller中了
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除channel
void EPollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃链接
void EPollPoller::fillActiveChannels(int          numEvents,
                                     ChannelList* activeChannels) const {
    // EventLoop拿到Poller给它，返回的发生事件的channel列表
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->emplace_back(channel);
    }
}

// 更新channel通道，其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel) {
    epoll_event event;
    ::memset(&event, 0, sizeof(event));

    int fd = channel->fd();

    event.events = channel->events();
    // event.data.fd = fd;
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d", errno);
        } else {
            LOG_FATAL("epoll_ctl add/mod error:%d", errno);
        }
    }
}
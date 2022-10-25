#pragma once

#include <sys/epoll.h>
#include <vector>

#include "Poller.h"
#include "Timestamp.h"

// epoll, epoll_create, epoll_ctl, epoll_wait
class Channel;

class EPollPoller : public Poller {
public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void      updateChannel(Channel* channel) override;
    void      removeChannel(Channel* channel) override;

private:
    static const int kInitEventListSize = 16;

    // 填写活跃连接
    void fillActiveChannels(int          numEvents,
                            ChannelList* activeChannels) const;
    // 更新channel通道（通过调用epoll_ctl）
    void update(int operation, Channel* Channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;  // epoll_create创建返回的fd
    EventList events_;  // 存放epoll_wait返回的所有发生事件的文件描述符集
};
#pragma once

#include <unordered_map>
#include <vector>

#include "Timestamp.h"
#include "noncopyable.h"

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    // 为所有IO复用模块保留统一接口
    // Poll the IO events
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    // 更改感兴趣的IO事件
    virtual void updateChannel(Channel* channel) = 0;
    // 当析构时移除通道
    virtual void removeChannel(Channel* channel) = 0;

    // 判断参数channel是否在当前Poller中
    bool hasChannel(Channel* channel) const;

    // EventLoop可以通过调用该接口获取默认的IO复用具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    //<sockfd, channelType>
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;

private:
    // 定义Poller所属的事件循环EventLoop
    EventLoop* ownerLoop_;
};
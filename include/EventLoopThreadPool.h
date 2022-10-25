#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "noncopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseloop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) {
        numThreads_ = numThreads;
    }
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 如果在多线程中，baseloop会默认以轮询方式分配Channel给subLoop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const {
        return started_;
    }
    const std::string name() const {
        return name_;
    }

private:
    // 用户创建的baseloop，如果线程为1则只有baseLoop，否则启用线程池
    EventLoop*  baseLoop_;
    std::string name_;
    bool        started_;
    int         numThreads_;
    int         next_;  // 轮询的下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*>                       loops_;
};
#include <memory>

#include "EventLoopThread.h"
#include "EventLoopThreadPool.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop*         baseLoop,
                                         const std::string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {}

// loop是栈上的变量，不需要析构函数来释放
EventLoopThreadPool::~EventLoopThreadPool() {}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[name_.size() + 32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));

        // 底层创建线程，绑定一个新的EventLoop并返回该loop的地址
        loops_.push_back(t->startLoop());
    }

    // 如果只有一个线程就运行baseLoop
    if (numThreads_ == 0 && cb) {
        cb(baseLoop_);
    }
}

// 如果在多线程中，baseLoop会默认以轮询方式分配Channel给subLoop
EventLoop* EventLoopThreadPool::getNextLoop() {
    // 如果只设置了一个线程，即只有一个mainReactor而无subReactor，那轮询只有一个结果
    EventLoop* loop = baseLoop_;

    // 通过轮询获取下一个处理事件的loop
    if (!loops_.empty()) {
        loop = loops_[next_];
        ++next_;
        if (next_ >= loops_.size()) {
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    } else {
        return loops_;
    }
}
#include "EventLoopThread.h"
#include "EventLoop.h"

// bind能够在绑定时候就同时绑定一部分参数，未提供的参数则使用占位符表示，然后在运行时传入实际的参数值。
// PS：绑定的参数将会以值传递的方式传递给具体函数，占位符将会以引用传递。众所周知，静态成员
// 函数其实可以看做是全局函数，而非静态成员函数则需要传递this指针作为第一个参数，所以std::bind能很容易地绑定成员函数。
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string&        name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    // 启用底层线程Thread类对象thread_中通过start()创建的线程
    thread_.start();
    EventLoop* loop = nullptr;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 调用wait时，若参数互斥量lock被锁定，则wait会阻塞
        // 只要其它线程调用notify_one()或者notify_all()函数，wait()就会结束阻塞
        while (loop_ == nullptr) {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

// 以下方法单独在新线程里运行
void EventLoopThread::threadFunc() {
    // 创建一个独立的EventLoop对象，和上面的thread是一一对应的
    // 即 one loop per thread
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();    //执行EventLoop的loop()，开启了底层的Poller的poll()
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <string>
#include <atomic>

#include "noncopyable.h"

class Thread : noncopyable {
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string &name = std::string());
    // make it movable in C++11
    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return  tid_;}
    const std::string &name() const {return name_; }

    static int numCreated() { return numCreated_; }
private:
    void setDefaultName();
    bool started_;
    bool joined_;

    std::shared_ptr<std::thread> thread_;
    pid_t tid_;     //创建线程时绑定
    ThreadFunc func_;   //线程回调函数
    
    std::string name_;
    static std::atomic_int numCreated_;
};
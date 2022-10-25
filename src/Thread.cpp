#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_{0};

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name) {}

Thread::~Thread() {
    if (started_ && !joined_) {
        // thread类提供了设置分离线程的方法，线程运行后自动销毁（非阻塞）
        thread_->detach();
    }
}

// 一个thread对象记录的就是一个新线程的详细信息
void Thread::start() {
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);  // false指不设置进程间共享

    // 开启线程，使用lambda表达式传递
    thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
        // 获取线程tid
        tid_ = CurrentThread::tid();
        // 信号量+1
        sem_post(&sem);
        // 开启一个新线程，专门执行该函数
        func_();
    }));
    // 必须等待获取上面新创建的子进程tid值
    sem_wait(&sem);
}

void Thread::join() {
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName() {
    int num = ++numCreated_;
    if (name_.empty()) {
        char buf[32]{0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
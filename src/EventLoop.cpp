#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <memory>

//__thread是GCC内置的线程局部存储措施，__thread修饰的变量在每个线程中有一份独立实例，各个线程的值互不干扰。
//__thread只能修饰POD类型,防止一个线程创建多个EventLoop。
// POD的全称是Plain Old Data，Plain表明它是一个普通的类型
// 没有虚函数虚继承等特性Old表明它与C兼容。
__thread EventLoop* t_loopInThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间10s
const int kPollTimeMs = 10000;

// 通过eventfd在线程之间传递数据的好处是多个线程之间不需要上锁就可以实现同步。
// 函数原型 int eventfd(unsigned int initval,int flags)
// eventfd可以用于同一个进程之中线程之间的通信，可用于亲缘进程之间的通信
// eventfd 是一个计数相关的fd。计数不为零是有可读事件发生
// read 之后计数会清零，write 则会递增计数器。eventfd 如果用 epoll
// 监听事件，那么都是监听读事件，因为监听写事件无意义（一直可写）

// 创建wakeupfd，用来notify唤醒subRactor处理新来的Channel
int createEventfd() {
    int evfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evfd < 0) {
        LOG_FATAL("create eventfd error:%d", errno);
    }
    return evfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      timerQueue_(new TimerQueue(this)),
      activeChannels_(NULL) {
    LOG_DEBUG("EventLoop created %p in thread %d", this, threadId_);
    if (t_loopInThisThread) {
        LOG_FATAL("Another EventLoop %p exists in this thread %d",
                  t_loopInThisThread, threadId_);
    } else {
        t_loopInThisThread = this;
    }
    // 设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(
        std::bind(&EventLoop::handleRead, this));
    // 任何EventLoop都将监听wakeupChannel_的EPOLL读事件
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    // 给Channel移除所有感兴趣的事件
    wakeupChannel_->disableAll();
    // 把Channel从EventLoop上删除掉
    wakeupChannel_->remove();

    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

// 开启事件循环
void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping", this);

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (auto channel : activeChannels_) {
            // Poller监听到哪些channel发生了事件 然后上报给EventLoop
            // 通知channel处理相应事件
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前EventLoop事件循环需要处理的回调操作
        // 对于线程数 >=2 的情况 IO线程 mainloop(mainReactor) 主要工作：
        // accept接收连接 => 将accept返回的connfd打包为Channel =>
        // TcpServer::newConnection
        // 通过轮询将TcpConnection对象分配给subloop处理
        // mainloop调用queueInLoop将回调加入subloop（该回调需要subloop执行
        // 但subloop还在poller_->poll处阻塞）
        // queueInLoop通过wakeup将subloop唤醒
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping.", this);
    looping_ = false;
}

// 退出事件循环
//   1.如果loop在自己的线程中quit成功了，说明当前线程已经执行完毕了loop()函数的poller_->poll并退出
//   2.如果不是当前EventLoop所属的线程中调用quit退出EventLoop，需要唤醒EventLoop所属的线程的epoll_wait
// 比如在一个subloop(worker)中调用mainloop(IO)的quit时，
// 需要唤醒mainloop(IO)的poller_->poll 让其执行完loop()函数 注意：
// 正常情况下 mainloop负责请求连接 将回调写入subloop中
// 通过生产者消费者模型即可实现线程安全的队列 ！！！
// 但是muduo通过wakeup()机制 使用eventfd创建的wakeupFd_ notify
// 使得mainloop和subloop之间能够进行通信
void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

// 立即唤醒当前loop所在线程并在当前loop中执行cb
void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {  // 在当前线程执行回调
        cb();
    } else {  // 在非EventLoop线程中执行cb,就需要唤醒EventLoop所在线程执行cb
        queueInLoop(cb);
    }
}

// 把回调函数放入队列 唤醒loop所在的线程执行cb
void EventLoop::queueInLoop(Functor cb) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // callingPendingFunctors的意思是 当前loop正在执行回调中
    // 但是loop的pendingFunctors_中又加入了新的回调
    // 需要通过wakeup写事件，唤醒相应的需要执行上面回调操作的loop的线程
    // 让loop()下一次poller_->poll()不再阻塞（阻塞的话会延迟前一次新加入的回调的执行）
    // 然后继续执行pendingFunctors_中的回调函数
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();  // 唤醒loop所在的线程
    }
}

// 通过eventfd唤醒loop所在的线程
void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t  n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleRead() write %lu bytes instead of 8",
                  n);
    }
}

// EentLoop的方法 => Poller的方法
void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel) {
    return poller_->hasChannel(channel);
}

// 处理读事件
void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t  n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",
                  n);
    }
}

// 执行上层的回调函数
void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        // 交换的方式减少了锁的临界区范围，提升效率，同时避免了死锁
        // 如果执行functor()在临界区内
        // 且functor()中调用queueInLoop()就会产生死锁
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor : functors) {
        functor();  // 执行当前loop所需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) {
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb) {
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId) {
    return timerQueue_->cancel(timerId);
}

void EventLoop::abortNotInLoopThread() {
    LOG_FATAL("EventLoop::abortNotInLoopThread - EventLoop %p was created "
              "in threadId_ = %d , current thread id = %d",
              this, threadId_, CurrentThread::tid());
}
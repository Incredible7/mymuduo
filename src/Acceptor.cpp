#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// 新建一个非阻塞的sockfd
static int createNonblocking() {
    int sockfd =
        ::socket(AF_INET, SOCK_NONBLOCK | SOCK_CLOEXEC | SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_FATAL("%s:%s:%d listen socket create err:%d", __FILE__,
                  __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr,
                   bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()) {
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);

    // TcpServer.start() -> Acceptor.listen
    // 有新用户连接，要执行一个回调
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

// 开始监听acceptSocket_
void Acceptor::listen() {
    listenning_ = true;
    acceptSocket_.listen();
    // 为Channel注册可读事件
    acceptChannel_.enableReading();
}

// listenfd有事件发生了，就是有新用户连接
void Acceptor::handleRead() {
    InetAddress peerAddr;
    int         connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            // 轮询找到subLoop并唤醒，分发当前新客户端的Channel
            // 如何分发？
            newConnectionCallback_(connfd, peerAddr);
        } else {
            ::close(connfd);
        }
    } else {
        LOG_ERROR("%s:%s:%d accept err:%d", __FILE__, __FUNCTION__,
                  __LINE__, errno);
        if (errno == EMFILE) { /* Too many open files */
            LOG_ERROR("%s:%s:%d sockfd reached limit", __FILE__,
                      __FUNCTION__, __LINE__);
        }
    }
}
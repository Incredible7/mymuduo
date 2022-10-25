#include "TcpClient.h"
#include "Connector.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Thread.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <mutex>

namespace detail {
void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn) {
    loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
}  // namespace detail

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        LOG_FATAL("%s:%s:%d mainLoop is null!", __FILE__, __FUNCTION__,
                  __LINE__);
    }
    return loop;
}

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr,
                     const std::string& nameArg)
    : loop_(CheckLoopNotNull(loop)),
      connector_(new Connector(loop, serverAddr)),
      messageCallback_(defaultMessageCallback),
      connectionCallback_(defaultConnectionCallback),
      retry_(false),
      connect_(true),
      nextConnId_(1) {
    connector_->setNewConnectionCallback(
        std::bind(&TcpClient::newConnection, this, std::placeholders::_1));
    LOG_INFO("TcpClient::TcpClient[%s] - connector %p", name_.c_str(),
             connector_.get());
}

TcpClient::~TcpClient() {
    LOG_INFO("TcpClient::~TcpClient[%s] - connector %p", name_.c_str(),
             connector_.get());
    TcpConnectionPtr conn;
    bool             unique = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        unique = connection_.unique();
        conn = connection_;
    }
    if (conn) {
        CloseCallback cb = std::bind(&detail::removeConnection, loop_,
                                     std::placeholders::_1);
        loop_->runInLoop(
            std::bind(&TcpConnection::setCloseCallback, conn, cb));
        if (unique) {
            conn->forceClose();
        }
    } else {
        connector_->stop();
        // loop->runAfter(1,std::bind(removeConnector,connector_));
    }
}

void TcpClient::connect() {
    LOG_INFO("TcpClient::connect[%s] - connecting to %s", name_.c_str(),
             connector_->serverAddress().toIpPort().c_str());
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect() {
    connect_ = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_->shutdown();
    }
}

void TcpClient::stop() {
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd) {
    sockaddr_in peeraddr;
    ::memset(&peeraddr, 0, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(sockfd, (sockaddr*)&peeraddr, &addrlen) < 0) {
        LOG_ERROR("sockets::getPeerAddr err.");
    }
    InetAddress peerAddr(peeraddr);
    char        buf[32];
    snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(),
             nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    sockaddr_in localaddr;
    ::memset(&localaddr, 0, sizeof localaddr);
    socklen_t addrlen1 = static_cast<socklen_t>(sizeof localaddr);
    if (::getsockname(sockfd, (sockaddr*)&localaddr, &addrlen1) < 0) {
        LOG_ERROR("sockets::getlocalAddr err.");
    }
    InetAddress localAddr(localaddr);

    TcpConnectionPtr conn(
        new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this,
                                     std::placeholders::_1));

    {
        std::unique_lock<std::mutex> lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn) {
    assert(loop_ == conn->getLoop());

    {
        std::unique_lock<std::mutex> lock(mutex_);
        assert(connection_ == conn);
        connection_.reset();
    }
    
    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    if (retry() && connect_) {
        LOG_INFO("TcpClient::connect[%s] - Reconnecting to %s",
                 name_.c_str(),
                 connector_->serverAddress().toIpPort().c_str());

        connector_->restart();
    }
}
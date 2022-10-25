#pragma once

#include "Connector.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "Thread.h"
#include "noncopyable.h"

#include <atomic>
#include <mutex>

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient : noncopyable {
public:
    TcpClient(EventLoop* loop, const InetAddress& serverAddr,
              const std::string& nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection() {
        std::unique_lock<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop* getLoop() const {
        return loop_;
    }
    bool retry() const {
        return retry_;
    }
    void enableRetry() {
        retry_ = true;
    }

    const std::string& name() const {
        return name_;
    }

    // 设置连接回调，非线程安全
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCallback_ = std::move(cb);
    }
    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(MessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        writeCompleteCallback_ = std::move(cb);
    }

private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& com);

    EventLoop*            loop_;
    ConnectorPtr          connector_;  // 避免暴露Connector
    const std::string     name_;
    ConnectionCallback    connectionCallback_;
    MessageCallback       messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;

    std::atomic_bool retry_;
    std::atomic_bool connect_;

    // always in loop thread
    int              nextConnId_;
    std::mutex       mutex_;
    TcpConnectionPtr connection_;  // 有mutex_，保证线程安全
};
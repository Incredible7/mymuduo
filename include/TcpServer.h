#pragma once

// 用户使用muduo编写服务器程序

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "noncopyable.h"

// 对外的服务器编程使用的类
class TcpServer {
public:
    // 线程初始回调
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option {
        kNoReusePort,
        kReusePort, 
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
              const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback& cb) {
        threadInitCallback_ = cb;
    }
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }

    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    // 有新连接到来
    void newConnection(int sockfd, const InetAddress& peerAddr);
    // 移除连接
    void removeConnection(const TcpConnectionPtr& conn);
    // 在当前事件循环移除连接
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    // 保存所有连接的map
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    // baseloop 用户自定义的loop
    EventLoop* loop_;

    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor>
        acceptor_;  // 运行在mainloop 任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    ConnectionCallback connectionCallback_;  // 有新连接时的回调
    MessageCallback messageCallback_;  // 有读写事件发生时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成后的回调

    ThreadInitCallback threadInitCallback_;  // loop线程初始化的回调

    std::atomic_int started_;

    // 下一个连接序号
    int nextConnId_;

    ConnectionMap connections_;  // 保存所有的连接
};
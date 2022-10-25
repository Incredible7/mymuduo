#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Logger.h"
#include "Timestamp.h"
#include "noncopyable.h"

class Channel;
class EventLoop;
class Socket;

// TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
// => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
    /**
     * shared_from_this成员函数便可获取到_Tp类型智能指针，_Tp类型就是我们的目标类型。
     * 再来看看std::enable_shared_from_this的构造函数都是protected，
     * 因此不能直接创建std::enable_from_shared_from_this类的实例变量，只能作为基类使用。
     * 当类A被share_ptr管理，且在类A的成员函数里需要把当前类对象作为参数传给其他函数时，
     * 就需要传递一个指向自身的share_ptr。我们就使类A继承enable_share_from_this，
     * 然后通过其成员函数share_from_this()返回当指向自身的share_ptr。
     **/
public:
    TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const {
        return loop_;
    }
    const std::string& name() const {
        return name_;
    }
    const InetAddress& localAddress() const {
        return localAddr_;
    }
    const InetAddress& peerAddress() const {
        return peerAddr_;
    }

    bool connected() const {
        return state_ == kConnected;
    }

    // 发送数据
    void send(const std::string& buf);
    void send(const void* msg, int len);
    void send(Buffer* buf);

    // 关闭连接
    void shutdown();
    void forceClose();
    void setTcpNoDelay(bool on);
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }
    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb,
                                  size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    // //这两个函数应该适应任何类型，目前仅适配FILE类型。将来可以设计any类来曼珠需求
    // const FILE &getContext() const
    // {
    //     return context_;
    // }

    // FILE *getMutableContext()
    // {
    //     return &context_;
    // }

    // 连接建立
    void connectEstablished();
    // 连接销毁
    void connectDestroyed();

private:
    enum StateE {
        kDisconnected,   // 连接已断开
        kConnecting,     // 连接中
        kConnected,      // 已连接，可通信
        kDisconnecting,  // 断开连接中
    };

    void setState(StateE state) {
        state_ = state;
    }

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const std::string& msg);
    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

    // 这里是baseLoop还是subLoop由TcpServer中创建的线程数决定
    // 若多为Reactor，此loop_指向subLoop；若为单Reactor，此loop_指向baseLoop
    EventLoop*        loop_;
    const std::string name_;
    std::atomic_int   state_;
    bool              reading_;

    // Socket Channel，这里和Acceptor类似
    // Acceptor => mainLoop  TcpConnection => subLoop
    std::unique_ptr<Socket>  socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 这些回调TcpServer也有，用户通过写入TcpServer注册
    // TcpServer再将注册的回调传递给TcpConnection
    // TcpConnection再将回调注册到Channel中
    ConnectionCallback connectionCallback_;  // 有新连接时的回调
    MessageCallback    messageCallback_;     // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完的回调
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback         closeCallback_;  // 关闭连接的回调
    size_t                highWaterMark_;

    // 数据缓冲区
    Buffer inputBuffer_;   // 接受数据缓冲区
    Buffer outputBuffer_;  // 发送数据缓冲区

    // 记录Tcp上下文
};

static void defaultConnectionCallback(const TcpConnectionPtr& conn) {
    std::string state;
    if (conn->connected()) {
        state = "UP";
    } else {
        state = "DOWN";
    }
    LOG_INFO("%s -> %s is %s", conn->peerAddress().toIpPort().c_str(),
             conn->localAddress().toIpPort().c_str(), state.c_str());
    // do not call conn->forceClose(), because some users want to register
    // message callback only.
}

static void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf,
                                   Timestamp) {
    buf->retrieveAll();
}
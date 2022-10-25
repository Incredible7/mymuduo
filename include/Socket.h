#pragma once

#include "noncopyable.h"

#include <netinet/tcp.h>

class InetAddress;

// 对socket file descriptor的封装
// 该类对象析构时会自动close sockfd
// 线程安全的类，所有操作都委托给操作系统
class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    Socket(Socket&&);
    ~Socket();

    int fd() const {
        return sockfd_;
    }

    // return true if sccess
    bool getTcpInfo(tcp_info* tcpi) const;
    bool getTcpInfoString(char* buf, int len) const;

    // abort if address in use
    void bindAddress(const InetAddress& localaddr);
    // abort if address in use
    void listen();

    // 成功时，返回一个非负整数，成功时，返回一个非负整数，该整数是已接受套接字的描述符
    // 该套接字已设置为非阻塞和关闭执行。*peeraddr已被分配且untouched，错误返回-1，
    int accept(InetAddress* peeraddr);

    void shutdownWrite();
    // 开启或关闭Nagle算法
    void setTcpNoDelay(bool on);
    // 开启或关闭SO_REUSEADDR选项
    void setReuseAddr(bool on);
    // 开启或关闭 SO_REUSEPORT选项
    void setReusePort(bool on);
    // 开启或关闭SO_KEEPALIVE选项
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};
#pragma once

#include "copyable.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// 封装sockaddr_in
class InetAddress : public copyable {
public:
    // 用给定的端口号构造一个InetAddress对象
    // 常在TcpServer监听时使用(listen)
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");

    // 用给定的IP和端口号构造一个InetAddress对象
    // 常在TcpClient使用
    explicit InetAddress(std::string ip, uint16_t port);

    // 为给定的C形式的地址结构体构造InetAddress对象
    explicit InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}

    sa_family_t family() const {
        return addr_.sin_family;
    }
    //只返回IP
    std::string toIp() const;
    //返回IP和端口
    std::string toIpPort() const;
    uint16_t    port() const;

    const sockaddr_in* getSockAddr() const {
        return &addr_;
    }
    void setSockAddr(const sockaddr_in& addr) {
        addr_ = addr;
    }

private:
    sockaddr_in addr_;
};
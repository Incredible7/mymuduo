#pragma once

#include <mymuduo/TcpServer.h>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, InetAddress &listenAddr);

    void start();

private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time);

    TcpServer server_;
};
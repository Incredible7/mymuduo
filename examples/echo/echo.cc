#include "echo.h"
#include <mymuduo/Logger.h>

using namespace std::placeholders;

EchoServer::EchoServer(EventLoop* loop, InetAddress& listenAddr)
    : server_(loop, listenAddr, "EchoServer") {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::start() {
    server_.start();
}

void EchoServer::onConnection(const TcpConnectionPtr& conn) {
    std::string state;
    if (conn->connected()) {
        state = "UP";
    } else {
        state = "DOWN";
    }
    LOG_INFO("EchoServer - %s -> %s is %s",
             conn->peerAddress().toIpPort().c_str(),
             conn->localAddress().toIpPort().c_str(), state.c_str());
}
void EchoServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                           Timestamp time) {
    std::string msg(buf->retrieveAllAsString());
    LOG_INFO("%s recevie %d bytes recevied at %s", conn->name().c_str(),
             msg.size(), time.toString().c_str());
    conn->send(msg);
}

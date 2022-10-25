#include "discard.h"
#include <mymuduo/Logger.h>

using namespace std::placeholders;

DiscardServer::DiscardServer(EventLoop *loop, const InetAddress &listenAddr)
    : server_(loop, listenAddr, "DiscardServer")
{
    server_.setConnectionCallback(
        std::bind(&DiscardServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&DiscardServer::onMessage, this, _1, _2, _3));
}

void DiscardServer::start()
{
    server_.start();
}

void DiscardServer::onConnection(const TcpConnectionPtr &conn)
{
    std::string state;
    if (conn->connected())
    {
        state = "UP";
    }
    else
    {
        state = "DOWN";
    }
    LOG_INFO("DiscardServer - %s -> %s is %s",
             conn->peerAddress().toIpPort().c_str(),
             conn->localAddress().toIpPort().c_str(), state.c_str());
}

void DiscardServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
{
    std::string msg(buf->retrieveAllAsString());
    LOG_INFO("%s discards %d bytes recevied at %s",
             conn->name().c_str(), msg.size(), time.toString().c_str());
}
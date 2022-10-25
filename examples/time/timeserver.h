#pragma once

#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

using namespace std::placeholders;
class TimeServer
{
public:
    TimeServer(EventLoop *loop, const InetAddress &listenAddr);

    void start();

private:
    void onConnection(const TcpConnectionPtr &conn);

    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp time);

    TcpServer server_;
};

TimeServer::TimeServer(EventLoop *loop,
                       const InetAddress &listenAddr)
    : server_(loop, listenAddr, "TimeServer")
{
    server_.setConnectionCallback(
        std::bind(&TimeServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&TimeServer::onMessage, this, _1, _2, _3));
}

void TimeServer::start()
{
    server_.start();
}

void TimeServer::onConnection(const TcpConnectionPtr &conn)
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
    LOG_INFO("TimeServer - %s -> %s is %s",
             conn->peerAddress().toIpPort().c_str(),
             conn->localAddress().toIpPort().c_str(), state.c_str());
    if (conn->connected())
    {
        time_t now = ::time(NULL);
        int32_t be32 = static_cast<int32_t>(now);
        conn->send(&be32, sizeof(be32));
        conn->shutdown();
    }
}

void TimeServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp time)
{
    std::string msg(buf->retrieveAllAsString());
    LOG_INFO("%s discards %d bytes received at %s", conn->name().c_str(), msg.size(), time.toString().c_str());
}
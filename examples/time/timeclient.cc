#include "mymuduo/Logger.h"
#include "mymuduo/TcpClient.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
using namespace std::placeholders;

class TimeClient : noncopyable
{
public:
    TimeClient(EventLoop *loop, const InetAddress &serverAddr)
        : loop_(loop),
          client_(loop, serverAddr, "TimeClient")
    {
        client_.setConnectionCallback(
            std::bind(&TimeClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&TimeClient::onMessage, this, _1, _2, _3));
        // client_.enableRetry();
    }

    void connect()
    {
        client_.connect();
    }

private:
    EventLoop *loop_;
    TcpClient client_;

    void onConnection(const TcpConnectionPtr &conn)
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
        if (!conn->connected())
        {
            loop_->quit();
        }
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buffer, Timestamp receiveTime)
    {
        if (buffer->readableBytes() >= sizeof(int32_t))
        {
            const void *data = buffer->peek();
            int32_t be32 = *static_cast<const int32_t *>(data);
            buffer->retrieve(sizeof(int32_t));
            time_t time = be32;
            Timestamp ts(time);
            LOG_INFO("Server time = %d , %s", time, ts.toString().c_str());
        }
        else
        {
            //这里写成buf会有重名问题
            LOG_INFO("%s no enough data %d at %s", conn->name().c_str(), buffer->readableBytes(), receiveTime.toString().c_str());
        }
    }
};

int main(int argc, char *argv[])
{
    LOG_INFO("pid = %d", getpid());
    if (argc > 1)
    {
        EventLoop loop;
        InetAddress serverAddr(argv[1], 2037);

        TimeClient timeClient(&loop, serverAddr);
        timeClient.connect();
        loop.loop();
    }
    else
    {
        printf("Usage: %s host_ip\n", argv[0]);
    }
}

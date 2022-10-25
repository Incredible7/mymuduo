#include <mymuduo/Logger.h>
#include <mymuduo/TcpClient.h>

#include <stdio.h>
#include <unistd.h>
#include <utility>

using namespace std::placeholders;

class ChargenClient : noncopyable {
public:
    ChargenClient(EventLoop* loop, const InetAddress& listenAddr)
        : loop_(loop), client_(loop, listenAddr, "ChargenClient") {
        client_.setConnectionCallback(
            std::bind(&ChargenClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&ChargenClient::onMessage, this, _1, _2, _3));
        // client_.enableRetry();
    }

    void connect() {
        client_.connect();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        std::string state;
        if (conn->connected()) {
            state = "UP";
        } else {
            state = "DOWN";
        }
        LOG_INFO("EchoServer - %s -> %s is %s",
                 conn->peerAddress().toIpPort().c_str(),
                 conn->localAddress().toIpPort().c_str(), state.c_str());
        if (!conn->connected())
            loop_->quit();
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                   Timestamp receiveTime) {
        buf->retrieveAll();
    }

    EventLoop* loop_;
    TcpClient  client_;
};

int main(int argc, char* argv[]) {
    LOG_INFO("pid = %d", getpid());
    if (argc > 1) {
        EventLoop   loop;
        InetAddress serverAddr(argv[1], 2015);

        ChargenClient chargenClient(&loop, serverAddr);
        chargenClient.connect();
        loop.loop();
    } else {
        printf("Usage: %s host_ip\n", argv[0]);
    }
}

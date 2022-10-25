#include "codec.h"

#include "EventLoopThread.h"
#include "Logger.h"
#include "TcpClient.h"
#include "noncopyable.h"

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <mutex>

using namespace std::placeholders;

class ChatClient : noncopyable {
public:
    ChatClient(EventLoop* loop, const InetAddress& serverAddr)
        : client_(loop, serverAddr, "ChatClient"),
          codec_(
              std::bind(&ChatClient::onStringMessage, this, _1, _2, _3)) {
        client_.setConnectionCallback(
            std::bind(&ChatClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
        client_.enableRetry();
    }

    void connect() {
        client_.connect();
    }

    void disconnect() {
        client_.disconnect();
    }

    void write(const std::string& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (connection_) {
            codec_.send(connection_.get(), message);
        }
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        std::string state;
        if (conn->connected()) {
            state = "UP";
        } else {
            state = "DOWN";
        }
        LOG_INFO("DayTimeServer - %s -> %s is %s",
                 conn->peerAddress().toIpPort().c_str(),
                 conn->localAddress().toIpPort().c_str(), state.c_str());
        std::unique_lock<std::mutex> lock(mutex_);
        if (conn->connected()) {
            connection_ = conn;
        } else {
            connection_.reset();
        }
    }

    void onStringMessage(const TcpConnectionPtr&,
                         const std::string& message, Timestamp) {
        printf("<<< %s\n", message.c_str());
    }

    TcpClient         client_;
    LengthHeaderCodec codec_;
    std::mutex        mutex_;
    TcpConnectionPtr  connection_;
};

int main(int argc, char* argv[]) {
    LOG_INFO("pid = %d", getpid());
    if (argc > 2) {
        EventLoopThread loopThread;
        uint16_t        port = static_cast<uint16_t>(atoi(argv[2]));
        InetAddress     serverAddr(argv[1], port);

        ChatClient client(loopThread.startLoop(), serverAddr);
        client.connect();
        std::string line;
        while (std::getline(std::cin, line)) {
            client.write(line);
        }
        client.disconnect();
        // CurrentThread::sleepUsec(1000 * 1000); // wait for disconnect,
        // see ace/logging/client.cc
    } else {
        printf("Usage: %s host_ip port\n", argv[0]);
    }
}

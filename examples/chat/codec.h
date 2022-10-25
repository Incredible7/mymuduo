#pragma once

#include "Buffer.h"
#include "Logger.h"
#include "TcpConnection.h"
#include "noncopyable.h"

class LengthHeaderCodec : noncopyable {
public:
    using StringMessageCallback = std::function<void(
        const TcpConnectionPtr&, const std::string& message, Timestamp)>;

    explicit LengthHeaderCodec(const StringMessageCallback& cb)
        : messageCallback_(cb) {}

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                   Timestamp receiveTime) {
        while (buf->readableBytes() >= kHeaderLen)  // kHeaderLen == 4
        {
            const void* data = buf->peek();
            int32_t     len = *static_cast<const int32_t*>(data);  // SIGBUS

            if (len > 65536 || len < 0) {
                LOG_ERROR("Invalid length %d", len);
                conn->shutdown();
                break;
            } else if (buf->readableBytes() >= len + kHeaderLen) {
                buf->retrieve(kHeaderLen);
                std::string message(buf->peek(), len);
                messageCallback_(conn, message, receiveTime);
                buf->retrieve(len);
            } else {
                break;
            }
        }
    }

    void send(TcpConnection* conn, const std::string& message) {
        Buffer buf;
        buf.append(message.data(), message.size());
        int32_t len = static_cast<int32_t>(message.size());
        buf.prepend(&len, sizeof len);
        conn->send(&buf);
    }

private:
    StringMessageCallback messageCallback_;
    const static size_t   kHeaderLen = sizeof(int32_t);
};
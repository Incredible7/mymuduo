#pragma once

#include <string>
#include <vector>

// 网络库缓冲区类型定义
//  |   prepend   |     readable    |  writeable  |
//  ^             ^                 ^
//  0        readerIndex_       writerIndex_

class Buffer {
public:
    // 往前添加数据，序列化消息时可以使用
    static const size_t kCheapPretend = 8;
    static const size_t kInitialSize = 1024;  // 1KB

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPretend + initialSize),
          readerIndex_(kCheapPretend),
          writerIndex_(kCheapPretend) {}

    // 可读字节数
    size_t readableBytes() const {
        return writerIndex_ - readerIndex_;
    }
    // 剩余可写字节数
    size_t writableBytes() const {
        return buffer_.size() - writerIndex_;
    }
    // 可向前添加的字节数
    size_t pretendableBytes() const {
        return readerIndex_;
    }
    // 缓冲区中可读数据的起始地址
    const char* peek() const {
        return begin() + readerIndex_;
    }

    // 回收len bits的内存
    void retrieve(size_t len) {
        if (len < readableBytes()) {
            // 说明应用只读取了可读缓冲区的一部分数据，长度为len，
            // 剩下readerIndex - len
            readerIndex_ += len;
        } else {
            retrieveAll();
        }
    }

    // 回收所有内存，变为可写
    void retrieveAll() {
        readerIndex_ = kCheapPretend;
        writerIndex_ = kCheapPretend;
    }

    // 把 onMessage 函数上报的 Buffer 数据转成 string 类型的数据返回
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }
    std::string retrieveAsString(size_t len) {
        std::string result(peek(), len);
        // 已经读出缓冲区中数据，所以复位缓冲区
        retrieve(len);
        return result;
    }

    // 确认缓冲区是否足够可写
    void ensureWritableBytes(size_t len) {
        if (writableBytes() < len) {
            makeSpace(len);  // 不够就扩容呗
        }
    }

    // 可写地址
    char* beginWrite() {
        return begin() + writerIndex_;
    }
    const char* beginWrite() const {
        return begin() + writerIndex_;
    }

    // 把[data, data+len]栈内存上的数据添加到 writable 缓冲区中
    void append(const char* data, size_t len) {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    // 向前增加缓冲区
    void prepend(const void* data, size_t len) {
        readerIndex_ -= len;
        const char* d = static_cast<const char*>(data);
        // 相当于从 readerIndex 开始向前拷贝了 len 个空数据
        std::copy(d, d + len, begin() + readerIndex_);
    }

    // 从fd上读取/发送数据
    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);

private:
    // 缓冲区定义数组
    std::vector<char> buffer_;
    size_t            readerIndex_;
    size_t            writerIndex_;

    // vector底层数组首元素地址，就是数组的起始地址
    char* begin() {
        return &*buffer_.begin();
    }
    const char* begin() const {
        return &*buffer_.begin();
    }

    // 申请空间
    void makeSpace(size_t len) {
        /**
         * | kCheapPrepend |xxx| reader | writer |
         * | kCheapPrepend | reader ｜          len          |
         **/
        // xxx标示reader中已读的部分
        // 如果已读的数据部分（可复用）+可写的部分不够用就得扩容
        if (writableBytes() + pretendableBytes() < len + kCheapPretend) {
            buffer_.resize(writerIndex_ + len);
        } else {
            size_t readable = readableBytes();
            // 把未读的数据拷贝并重新置位
            std::copy(begin() + readerIndex_, begin() + writerIndex_,
                      begin() + kCheapPretend);
            readerIndex_ = kCheapPretend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
};
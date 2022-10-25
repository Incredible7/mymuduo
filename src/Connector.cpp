#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

const int Connector::kMaxRetryDelayMs;

int getSocketError(int sockfd) {
    int       optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof optval);

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }
}

// 使用这两个函数，我们可以通过套接字描述符来获取自己的IP地址和连接对端的IP地址.
// 如在未调用bind函数的TCP客户端程序上，可以通过调用getsockname()函数
// 获取由内核赋予该连接的本地IP地址和本地端口号，还可以在TCP的服务器端accept成功后，
// 通过getpeername()函数来获取当前连接的客户端的IP地址和端口号。
static sockaddr_in getLocalAddr(int sockfd) {
    sockaddr_in localaddr;
    ::memset(&localaddr, 0, sizeof localaddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
    if (::getsockname(sockfd, (sockaddr*)&localaddr, &addrlen) < 0) {
        LOG_ERROR("sockets::getLocalAddr error!");
    }
    return localaddr;
}

static struct sockaddr_in getPeerAddr(int sockfd) {
    struct sockaddr_in peeraddr;
    ::memset(&peeraddr, 0, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);
    if (::getpeername(sockfd, (sockaddr*)&peeraddr, &addrlen) < 0) {
        LOG_ERROR("sockets::getPeerAddr error!");
    }
    return peeraddr;
}

bool isSelfConnect(int sockfd) {
    struct sockaddr_in localaddr = getLocalAddr(sockfd);
    struct sockaddr_in peeraddr = getPeerAddr(sockfd);
    if (localaddr.sin_family == AF_INET) {
        const struct sockaddr_in* laddr4 =
            reinterpret_cast<struct sockaddr_in*>(&localaddr);
        const struct sockaddr_in* raddr4 =
            reinterpret_cast<struct sockaddr_in*>(&peeraddr);
        return laddr4->sin_port == raddr4->sin_port &&
               laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;
    } else {
        return false;
    }
}

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs) {
    LOG_DEBUG("constructor[%p]", this);
}

Connector::~Connector() {
    LOG_DEBUG("destructor[%p]", this);
}

void Connector::start() {
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

void Connector::startInLoop() {
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG("do not connect!");
    }
}

void Connector::stop() {
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

void Connector::stopInLoop() {
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect() {
    int sockfd = ::socket(
        AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        LOG_FATAL("sockets::createNonblockingOrDie");
    }
    int ret = ::connect(sockfd, (sockaddr*)serverAddr_.getSockAddr(),
                        static_cast<socklen_t>(sizeof(sockaddr_in)));
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN: /* Transport endpoint is already connected */
        connecting(sockfd);
        break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH: /* Network is unreachable */
        retry(sockfd);
        break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK: /* Socket operation on non-socket */
        // LOG_SYSERR << "connect error in Connector::startInLoop " <<
        // savedErrno;
        if (::close(sockfd) < 0) {
            LOG_ERROR("sockets::close");
        }
        break;

    default:
        // LOG_SYSERR << "Unexpected error in Connector::startInLoop " <<
        // savedErrno;
        if (::close(sockfd) < 0) {
            LOG_ERROR("sockets::close");
        }
        // connectErrorCallback_();
        break;
    }
}

void Connector::restart() {
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::connecting(int sockfd) {
    setState(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback(
        std::bind(&Connector::handleWrite, this));  // FIXME: unsafe
    channel_->setErrorCallback(
        std::bind(&Connector::handleError, this));  // FIXME: unsafe

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop(
        std::bind(&Connector::resetChannel, this));  // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel() {
    channel_.reset();
}

void Connector::handleWrite() {
    // LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = getSocketError(sockfd);
        if (err) {
            // LOG_WARN << "Connector::handleWrite - SO_ERROR = "<< err << "
            // " << strerror_tl(err);
            retry(sockfd);
        } else if (isSelfConnect(sockfd)) {
            // LOG_WARN << "Connector::handleWrite - Self connect";
            retry(sockfd);
        } else {
            setState(kConnected);
            if (connect_) {
                newConnectionCallback_(sockfd);
            } else {
                if (::close(sockfd) < 0) {
                    LOG_ERROR("sockets::close");
                }
            }
        }
    } else {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError() {
    LOG_ERROR("Connector::handleError state=%d", state_);
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = getSocketError(sockfd);
        // LOG_TRACE("SO_ERROR =%d %d ", err, strerror_tl(err));
        retry(sockfd);
    }
}

void Connector::retry(int sockfd) {
    if (::close(sockfd) < 0) {
        LOG_ERROR("sockets::close");
    }
    setState(kDisconnected);
    if (connect_) {
        LOG_INFO(
            "Connector::retry - Retry connecting to %s in %d milliseconds.",
            serverAddr_.toIpPort().c_str(), retryDelayMs_);
        loop_->runAfter(
            retryDelayMs_ / 1000.0,
            std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    } else {
        LOG_DEBUG("do not connect");
    }
}
#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Socket::~Socket() {
    ::close(sockfd_);
}

bool Socket::getTcpInfo(tcp_info* tcpi) const {
    socklen_t len = sizeof(*tcpi);
    ::memset(tcpi, 0, len);
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

bool Socket::getTcpInfoString(char* buf, int len) const {
    tcp_info tcpi;
    bool     ok = getTcpInfo(&tcpi);
    if (ok) {
        snprintf(
            buf, len,
            "unrecovered=%u "
            "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
            "lost=%u retrans=%u rtt=%u rttvar=%u "
            "sshthresh=%u cwnd=%u total_retrans=%u",
            // 重传数，表示当前待重传的包数，这个值在重传完毕后清零
            tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
            // 重传超时时间，这个和RTT有关系，RTT越大，rto越大
            tcpi.tcpi_rto,  // Retransmit timeout in usec
            // 用来延时确认的估值，单位为微秒.
            tcpi.tcpi_ato,  // Predicted tick of soft clock in usec
            // 本端的MSS; 对端的MSS
            tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss,
            // 本端在发送出去被丢失的报文数。重传完成后清零
            tcpi.tcpi_lost,  // Lost packets
            // 重传且未确认的数据段数
            tcpi.tcpi_retrans,  // Retransmitted packets out
            // RTT，往返时间
            tcpi.tcpi_rtt,  // Smoothed round trip time in usec
            // 描述RTT的平均偏差，该值越大，说明RTT抖动越大
            tcpi.tcpi_rttvar,  // Medium deviation
            // 拥塞控制慢开始阈值; 拥塞控制窗口大小
            tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,
            // 统计总重传的包数，持续增长。
            tcpi.tcpi_total_retrans);  // Total retransmits for entire
                                       // connection
    }
    return ok;
}

void Socket::bindAddress(const InetAddress& localaddr) {
    int ret = ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(),
                     sizeof(sockaddr_in));
    if (ret != 0) {
        LOG_FATAL("bind sockfd:%d failed", sockfd_);
    }
}

void Socket::listen() {
    int ret = ::listen(sockfd_, 1024);
    if (ret != 0) {
        LOG_FATAL("listen sockfd:%d failed.", sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr) {
    // Reactor模型, one loop per thread
    // poller + non-blocking IO

    sockaddr_in addr;
    socklen_t   len = sizeof(addr);
    ::memset(&addr, 0, sizeof(addr));

    int connfd =
        ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connfd >= 0) {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() {
    if(::shutdown(sockfd_, SHUT_WR) < 0) {
        LOG_ERROR("shutdownWrite error!");
    }
}

void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1:0;
    ::setsockopt(sockfd_, SOL_SOCKET, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}
#include "Socket.h"

#include <fmt/format.h>
#include <netinet/tcp.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "InetAddress.h"
#include "SocketsOps.h"

namespace muduo {

Socket::~Socket()
{
    if (sockfd_ >= 0) {
        sockets::close(sockfd_);
    }
}

Socket::Socket(Socket&& other) noexcept : sockfd_(other.sockfd_)
{
    other.sockfd_ = -1;  // 将其他 Socket 的 fd 置为无效
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other) {
        if (sockfd_ >= 0) {
            sockets::close(sockfd_);
        }
        sockfd_ = other.sockfd_;
        other.sockfd_ = -1;  // 将其他 Socket 的 fd 置为无效
    }
    return *this;
}

bool Socket::getTcpInfo(struct tcp_info* info) const
{
    socklen_t len = sizeof(*info);
    return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, info, &len) == 0;
}

std::optional<std::string> Socket::getTcpInfoString() const
{
    struct tcp_info info;
    if (getTcpInfo(&info)) {
        return fmt::format(
            "unrecovered={} "
            "rto={} ato={} snd_mss={} rcv_mss={} "
            "lost={} retrans={} rtt={} rttvar={} "
            "sshthresh={} cwnd={} total_retrans={}",
            info.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
            info.tcpi_rto,          // Retransmit timeout in usec
            info.tcpi_ato,          // Predicted tick of soft clock in usec
            info.tcpi_snd_mss, info.tcpi_rcv_mss,
            info.tcpi_lost,     // Lost packets
            info.tcpi_retrans,  // Retransmitted packets out
            info.tcpi_rtt,      // Smoothed round trip time in usec
            info.tcpi_rttvar,   // Medium deviation
            info.tcpi_snd_ssthresh, info.tcpi_snd_cwnd,
            info.tcpi_total_retrans);  // Total retransmits for entire connection
    }

    return std::nullopt;
}

void Socket::bindAddress(const InetAddress& localaddr) { sockets::bindOrDie(sockfd_, localaddr.getSockAddr()); }

void Socket::listen() { sockets::listenOrDie(sockfd_); }

int Socket::accept(InetAddress& peeraddr)
{
    struct sockaddr_in6 addr;
    std::memset(&addr, 0, sizeof addr);
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peeraddr.setSockAddrInet6(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() { sockets::shutdownWrite(sockfd_); }

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    if (::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval) < 0) {
        SPDLOG_ERROR("setTcpNoDelay failed: {}", strerror(errno));
    }
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReusePort(bool on)
{
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        SPDLOG_ERROR("SO_REUSEPORT failed.");
    }
#else
    if (on) {
        SPDLOG_ERROR("SO_REUSEPORT is not supported.");
    }
#endif
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

}  // namespace muduo
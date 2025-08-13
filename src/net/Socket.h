#ifndef MUDUO_NET_SOCKET_H
#define MUDUO_NET_SOCKET_H

#include <base/noncopyable.h>

#include <optional>
#include <string>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo {

class InetAddress;

class Socket : public noncopyable
{
   public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();
    
    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;
    
    int fd() const { return sockfd_; }
    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    std::optional<std::string> getTcpInfoString() const;

    /*
     abort if address in use
    */
    void bindAddress(const InetAddress& localaddr);
    /*
    abort if address in use
    */
    void listen();

    /*
      On success, returns a non-negative integer that is
     a descriptor for the accepted socket, which has been
     set to non-blocking and close-on-exec. *peeraddr is assigned.
     On error, -1 is returned, and *peeraddr is untouched.
    */
    int accept(InetAddress& peeraddr);

    void shutdownWrite();

    /*
     Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    */
    void setTcpNoDelay(bool on);

    /*
     Enable/disable SO_REUSEADDR
    */
    void setReuseAddr(bool on);

    /*
     Enable/disable SO_REUSEPORT
    */
    void setReusePort(bool on);

    /*
     Enable/disable SO_KEEPALIVE
    */
    void setKeepAlive(bool on);

   private:
    int sockfd_;
};

}  // namespace muduo

#endif  // MUDUO_NET_SOCKET_H
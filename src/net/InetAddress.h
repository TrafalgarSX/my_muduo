#ifndef MUDUO_NET_INETADDRESS_H
#define MUDUO_NET_INETADDRESS_H

#include <netinet/in.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace muduo {

namespace sockets {
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
}  // namespace sockets

class InetAddress
{
   public:
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);
    InetAddress(std::string_view ip, uint16_t port, bool ipv6 = false);
    explicit InetAddress(const struct sockaddr_in& addr);
    explicit InetAddress(const struct sockaddr_in6& addr);

    sa_family_t family() const;
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t port() const;

    const struct sockaddr* getSockAddr() const;
    void setSockAddrInet6(const struct sockaddr_in6& addr6);

    uint32_t ipv4NetEndian() const;
    uint16_t portNetEndian() const;

    static std::optional<InetAddress> resolve(std::string_view hostname);

    // set IPv6 ScopeID
    void setScopeId(uint32_t scope_id);

   private:
    union
    {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };
};

}  // namespace muduo

#endif  // MUDUO_NET_INETADDRESS_H
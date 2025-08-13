#include "InetAddress.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <string>
#include <string_view>

#include "SocketsOps.h"

static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

namespace muduo {

InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6) {
  static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
  static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");
  if (ipv6)
  {
    std::memset(&addr6_, 0, sizeof addr6_);
    addr6_.sin6_family = AF_INET6;
    in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
    addr6_.sin6_addr = ip;
    addr6_.sin6_port = htobe16(port);
  }
  else
  {
    std::memset(&addr_, 0, sizeof addr_);
    addr_.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
    addr_.sin_addr.s_addr = htobe32(ip);
    addr_.sin_port = htobe16(port);
  }
}
InetAddress::InetAddress(std::string_view ip, uint16_t port, bool ipv6) {
  std::string ip_str(ip);  // 确保 ip 是一个 std::string，避免潜在的生命周期问题

  if (ipv6 || ip.find(':') != std::string_view::npos)  // 检查是否包含 ':'，如果是 IPv6 地址
  {
    std::memset(&addr6_, 0, sizeof addr6_);
    sockets::fromIpPort(ip_str.c_str(), port, &addr6_);
  }
  else
  {
    std::memset(&addr_, 0, sizeof addr_);
    sockets::fromIpPort(ip_str.c_str(), port, &addr_);
  }

}

InetAddress::InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}
InetAddress::InetAddress(const struct sockaddr_in6& addr) : addr6_(addr) {}

sa_family_t InetAddress::family() const { return addr_.sin_family; }
std::string InetAddress::toIp() const
{
    char buf[64] = "";
    sockets::toIp(buf, sizeof buf, getSockAddr());
    return buf;
}
std::string InetAddress::toIpPort() const
{
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof buf, getSockAddr());
    return buf;
}
uint16_t InetAddress::port() const { return be16toh(portNetEndian()); }

const struct sockaddr* InetAddress::getSockAddr() const { return sockets::sockaddr_cast(&addr6_); }

void InetAddress::setSockAddrInet6(const struct sockaddr_in6& addr6) { addr6_ = addr6; }

uint32_t InetAddress::ipv4NetEndian() const { return addr_.sin_addr.s_addr; }
uint16_t InetAddress::portNetEndian() const { return addr_.sin_port; }

static thread_local char t_resolveBuffer[8 * 1024];

std::optional<InetAddress> InetAddress::resolve(std::string_view hostname)
{
    InetAddress out;
    struct hostent hent;
    struct hostent* he = NULL;
    int herrno = 0;
    std::memset(&hent, 0, sizeof(hent));
    
    std::string hostname_str(hostname);

    int ret = gethostbyname_r(hostname_str.data(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
    if (ret == 0 && he != NULL) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out.addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        return out;
    } else {
        if (ret) {
            SPDLOG_ERROR("InetAddress::resolve");
        }
        return std::nullopt;
    }
}

// set IPv6 ScopeID
void InetAddress::setScopeId(uint32_t scope_id)
{
    if (family() == AF_INET6) {
        addr6_.sin6_scope_id = scope_id;
    }
}

}  // namespace muduo
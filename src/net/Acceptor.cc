#include "Acceptor.h"

#include <errno.h>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <unistd.h>

#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

namespace muduo {

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
      acceptChannel_(loop, acceptSocket_.fd()),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback([this](Timestamp) { handleRead(); });
}

Acceptor::~Acceptor()
{
    SPDLOG_DEBUG("Acceptor destroyed: {}", static_cast<void*>(this));
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

/*
    Acceptor::handleRead()的策略很简单，每次accept(2)一个socket。另外还有两种实现策略:
    一是每次循环accept(2)，直至没有新的连接到达；
    二是每次尝试accept(2)N个新连接，N的值一般是10。
    后面这两种做法适合短连接服务，而muduo是为长连接服务优化的，因此这里用了最简单的办法。
*/
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(peerAddr);
    if (connfd >= 0) {
        Socket newSocket(connfd);
        if (newConnectionCallback_) {
            newConnectionCallback_(std::move(newSocket), peerAddr);
        }
        SPDLOG_DEBUG("New connection accepted: fd = {}, addr = {}", connfd, peerAddr.toIpPort());
    } else {
        SPDLOG_ERROR("Failed to accept new connection: {}", strerror(errno));
        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        // By Marc Lehmann, author of libev.
        // TODO 这是一个特殊问题，可能是因为文件描述符耗尽了。
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

}  // namespace muduo
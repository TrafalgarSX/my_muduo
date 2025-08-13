#include "Connector.h"

#include <spdlog/spdlog.h>

#include "Channel.h"
#include "EventLoop.h"
#include "SocketsOps.h"

namespace muduo {

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop), serverAddr_(serverAddr), state_(kDisconnected), retryDelayMs_(kInitRetryDelayMs)
{
    SPDLOG_DEBUG("Connector created: {}, server address: {}", static_cast<void*>(this), serverAddr.toIpPort());
}

Connector::~Connector()
{
    SPDLOG_DEBUG("Connector destroyed: {}, server address: {}", static_cast<void*>(this), serverAddr_.toIpPort());
    assert(!channel_);
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop([this]() { startInLoop(); });  // FIXME: unsafe
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_) {
        connect();
    } else {
        SPDLOG_DEBUG("Connector {} not connecting, connect_ is false", static_cast<void*>(this));
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop([this]() { stopInLoop(); });  // FIXME: unsafe
                                                     // FIXME: cancel timer
}

void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

void Connector::connect()
{
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            SPDLOG_ERROR("connect error in Connector::startInLoop: {}, fd = {}", strerror(savedErrno), sockfd);
            sockets::close(sockfd);
            break;

        default:
            SPDLOG_ERROR("Unexpected error in Connector::startInLoop: {}, fd = {}", strerror(savedErrno), sockfd);
            sockets::close(sockfd);
            break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);
    assert(!channel_);
    channel_.reset(new Channel(loop_, sockfd));
    channel_->setWriteCallback([this](){ handleWrite();});  // FIXME: unsafe
    channel_->setErrorCallback([this](){ handleError();});  // FIXME: unsafe

    // channel_->tie(shared_from_this()); is not working,
    // as channel_ is not managed by shared_ptr
    channel_->enableWriting();
}

int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop([this](){ resetChannel();});  // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel() { channel_.reset(); }

void Connector::handleWrite()
{
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        if (err) {
            SPDLOG_ERROR("SO_ERROR = {}", err);
            retry(sockfd);
        } else if (sockets::isSelfConnect(sockfd)) {
            SPDLOG_WARN("Connector::handleWrite - Self connect, fd = {}", sockfd);
            retry(sockfd);
        } else {
            setState(kConnected);
            if (connect_) {
                newConnectionCallback_(sockfd);
            } else {
                sockets::close(sockfd);
            }
        }
    } else {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);
    if (connect_) {
        SPDLOG_INFO("Connector::retry - Retry connecting to {} in {} milliseconds.",
                     serverAddr_.toIpPort(), retryDelayMs_);
        loop_->runAfter(retryDelayMs_ / 1000.0, std::bind(&Connector::startInLoop, shared_from_this()));
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    } else {
        SPDLOG_DEBUG("do not connect");
    }
}

}  // namespace muduo
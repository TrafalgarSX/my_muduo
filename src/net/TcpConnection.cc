#include "TcpConnection.h"

#include <errno.h>

#include <memory>
#include <string>
#include <string_view>

#include "Buffer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Socket.h"
#include "SocketsOps.h"

namespace muduo {

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, Socket&& socket, const InetAddress& localAddr,
                             const InetAddress& peerAddr)
    : loop_(loop),
      name_(name),
      socket_(std::make_unique<Socket>(std::move(socket))),
      channel_(std::make_unique<Channel>(loop, socket_->fd())),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024)  // 默认高水位64KB
{
    channel_->setReadCallback([this](Timestamp receiveTime) { handleRead(receiveTime); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
    socket_->setKeepAlive(true);  // TODO 这个有什么用？
    SPDLOG_DEBUG("TcpConnection {} created, fd: {}", name_, socket_->fd());
}
TcpConnection::~TcpConnection()
{
    SPDLOG_DEBUG("TcpConnection {} destroyed, fd: {}", name_, socket_->fd());
    assert(state_ == StateE::kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const { return socket_->getTcpInfo(tcpi); }

std::optional<std::string> TcpConnection::getTcpInfoString() const { return socket_->getTcpInfoString(); }

void TcpConnection::send(std::string&& message)
{
    if (state_ == StateE::kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(std::move(message));
        } else {
            // 移动捕获，避免拷贝
            loop_->runInLoop([this, msg = std::move(message)]() { sendInLoop(msg); });
        }
    }
}

void TcpConnection::send(const void* data, int len) { send(std::string_view(static_cast<const char*>(data), len)); }

void TcpConnection::send(std::string_view message)
{
    if (state_ == StateE::kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            // C++14 init capture：捕获时创建 std::string 副本
            loop_->runInLoop([this, msg = std::string(message)]() { sendInLoop(msg); });
        }
    }
}

void TcpConnection::send(Buffer* buf)
{
    if (state_ == StateE::kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        } else {
            loop_->runInLoop([this, message = buf->retrieveAllAsString()]() { sendInLoop(message); });
        }
    }
}

// 看起来没有必要
void TcpConnection::send(Buffer&& buf)
{
    if (state_ == StateE::kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf.peek(), buf.readableBytes());
            buf.retrieveAll();
        } else {
            loop_->runInLoop([this, message = buf.retrieveAllAsString()]() { sendInLoop(message); });
        }
    }
}

void TcpConnection::sendInLoop(std::string_view message) { sendInLoop(message.data(), message.size()); }

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    ;
    if (state_ == StateE::kDisconnected) {
        SPDLOG_ERROR("{} is disconnected, not sending", name_);
        return;
    }
    // if no thing in output queue, try writing directly
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop([this, conn = shared_from_this()]() { writeCompleteCallback_(conn); });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET)  // FIXME: any others?
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop([this, conn = shared_from_this(), oldLen, newLen = oldLen + remaining]() {
                highWaterMarkCallback_(conn, newLen);
            });
        }
        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == StateE::kConnected) {
        setState(StateE::kDisconnecting);
        // FIXME: shared_from_this()?
        loop_->runInLoop([this]() { shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        // we are not writing
        socket_->shutdownWrite();
    }
}

// 主动关闭连接
void TcpConnection::forceClose()
{
    if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
        setState(StateE::kDisconnecting);
        loop_->queueInLoop([this, conn = shared_from_this()]() {
            /*
             conn 确保在调用 forceCloseInLoop() 时仍然有效
             这里使用 shared_from_this() 确保在异步调用中 conn 不会被销毁
             这样可以安全地在 IO 线程中调用 forceCloseInLoop()
             在 IO 线程中强制关闭连接这将触发 handleClose()，并最终调用 closeCallback_
            */
            forceCloseInLoop();
        });
    }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
        setState(StateE::kDisconnecting);
        /*
         在指定的延迟后强制关闭连接, 因为有延迟，所以调用这个 lambda 的时候可能连接已经被销毁了
         这里使用 weak_from_this() 确保在异步调用中如果 conn 已经被销毁，不会导致访问已释放的内存
         如果 conn 已经被销毁，lock() 将返回 nullptr
         如果 conn 仍然有效，则调用 forceCloseInLoop() 来强制关闭
         这样可以确保在 IO 线程中安全地调用 forceCloseInLoop()
         这将触发 handleClose()，并最终调用 closeCallback_
        */
        loop_->runAfter(seconds, [this, conn = shared_from_this()->weak_from_this()]() {
            auto lockedConn = conn.lock();
            if (lockedConn) {  // 确保 conn 在调用时仍然有效
                lockedConn->forceCloseInLoop();
            }
        });
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == StateE::kConnected || state_ == StateE::kDisconnecting) {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}

const char* TcpConnection::stateToString() const
{
    switch (state_) {
        case StateE::kDisconnected:
            return "kDisconnected";
        case StateE::kConnecting:
            return "kConnecting";
        case StateE::kConnected:
            return "kConnected";
        case StateE::kDisconnecting:
            return "kDisconnecting";
        default:
            return "unknown state";
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);  // TODO 这个函数的作用是什么？
}

void TcpConnection::startRead()
{
    loop_->runInLoop([this]() { startReadInLoop(); });
}

void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading()) {
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopRead()
{
    loop_->runInLoop([this]() { startReadInLoop(); });
}

void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if (reading_ || channel_->isReading()) {
        channel_->disableReading();
        reading_ = false;
    }
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == StateE::kConnecting);
    setState(StateE::kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == StateE::kConnected) {
        setState(StateE::kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }

    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {  // 对端关闭连接
        handleClose();
    } else {
        SPDLOG_ERROR("TcpConnection::handleRead() - {}: {}", name_, strerror(savedErrno));
        errno = savedErrno;
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = sockets::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n > 0) {
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();  // 如果输出缓冲区已空，禁用写事件
                if (writeCompleteCallback_) {
                    loop_->queueInLoop([this, conn = shared_from_this()]() {
                        writeCompleteCallback_(conn);  // 调用写完成回调
                    });
                }
                if (state_ == StateE::kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            SPDLOG_ERROR("write error: {}", strerror(errno));
        }
    }
}

// 可用于处理连接关闭事件（主动或被动）
// TODO 为什么可以用于主动断开链接？
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    assert(state_ == StateE::kConnected || state_ == StateE::kDisconnecting);
    setState(StateE::kDisconnected);

    channel_->disableAll();

    // TODO 这里需要注意，连接断开时可能会有未处理的消息
    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    // must be the last line
    // 这里实际上调用的就是 connectDestroyed
    closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    SPDLOG_ERROR("[{}] - SO_ERROR = {}", name_, err);
}

}  // namespace muduo

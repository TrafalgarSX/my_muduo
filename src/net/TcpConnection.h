#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <base/noncopyable.h>

#include <any>
#include <memory>
#include <string>
#include <string_view>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"

struct tcp_info;

namespace muduo {

class Channel;
class EventLoop;
class Socket;

/*
    TCP connection, for both client and server usage.
    This is an interface class, so don't expose too much details.
*/
class TcpConnection : public noncopyable, public std::enable_shared_from_this<TcpConnection>
{
   public:
    TcpConnection(EventLoop* loop, const std::string& name, Socket&& socket, const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }

    const std::string& name() const { return name_; }

    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == StateE::kConnected; }
    bool disconnected() const { return state_ == StateE::kDisconnected; }

    // return true if success.
    bool getTcpInfo(struct tcp_info*) const;
    std::optional<std::string> getTcpInfoString() const;

    void send(std::string&& message);  // C++11
    void send(const void* message, int len);
    void send(std::string_view message);  // C++17
    void send(Buffer&& message);          // C++11
    void send(Buffer* message);           // this one will swap data

    void shutdown();  // NOT thread safe, no simultaneous calling
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);

    void setTcpNoDelay(bool on);

    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const { return reading_; };  // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const std::any& context) { context_ = context; }
    void setContext(std::any&& context) { context_ = std::move(context); }

    const std::any& getContext() const { return context_; }

    std::any* getMutableContext() { return &context_; }

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }

    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }

    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    /// Advanced interface
    Buffer* inputBuffer() { return &inputBuffer_; }

    Buffer* outputBuffer() { return &outputBuffer_; }

    /// Internal use only.
    void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }

    // called when TcpServer accepts a new connection
    void connectEstablished();  // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed();  // should be called only once

   private:
    enum class StateE {
        kDisconnected,  // 未连接
        kConnecting,    // 正在连接
        kConnected,     // 已连接
       kDisconnecting 
    };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(std::string_view message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }
    const char* stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop* loop_;                    // IO线程
    const std::string name_;             // 连接名称
    StateE state_{StateE::kConnecting};  // 连接状态
    bool reading_{false};                // 是否正在读取数据

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 回调函数
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;  // 高水位回调
    CloseCallback closeCallback_;

    size_t highWaterMark_;
    // TODO 为什么 Buffer 是 read write 在一个缓冲区里，但是这里仍然使用两个 Buffer？
    Buffer inputBuffer_;   // 输入缓冲区
    Buffer outputBuffer_;  // 输出缓冲区
    std::any context_;
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
}  // namespace muduo

#endif  // MUDUO_NET_TCPCONNECTION_H
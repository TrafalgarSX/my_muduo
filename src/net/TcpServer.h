#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include <base/noncopyable.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "Buffer.h"
#include "Callbacks.h"
#include "Socket.h"
#include "TcpConnection.h"

namespace muduo {

class EventLoop;
class Acceptor;
class InetAddress;
class EventLoopThreadPool;

class TcpServer : public noncopyable
{
   public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum class Option {
        kNoReusePort,
        kReusePort,
    };
    TcpServer(EventLoop* loop, const std::string& name, const InetAddress& listenAddr,
              Option option = Option::kNoReusePort);
    ~TcpServer();

    const std::string& ipPort() const { return ipPort_; }
    const std::string& name() const { return name_; }
    EventLoop* getLoop() const { return loop_; }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    void setThreadNum(int numThreads);

    std::shared_ptr<EventLoopThreadPool> threadPool();

    /// Starts the server if it's not listening.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    void start();

    void setConnectionCallback(ConnectionCallback cb);

    void setMessageCallback(MessageCallback cb);

    void setWriteCompleteCallback(WriteCompleteCallback cb);

    void setThreadInitCallback(ThreadInitCallback cb);

   private:
    /// Not thread safe, but in loop
    void newConnection(int sockfd, const InetAddress& peerAddr);
    /// Thread safe.
    void removeConnection(const TcpConnectionPtr& conn);
    /// Not thread safe, but in loop
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

   private:
    void newConnection(Socket, const InetAddress& peerAddr);
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;  // 假设有一个 Connection 类

    EventLoop* loop_;  // 服务器的事件循环
    const std::string ipPort_;
    const std::string name_;              // 服务器名称
    std::unique_ptr<Acceptor> acceptor_;  // 接收新连接的 Acceptor
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    std::atomic_int32_t started_{0};  // 服务器启动计数

    ConnectionCallback connectionCallback_;  // 连接建立或断开时的回调
    MessageCallback messageCallback_;        // 消息接收时的回调
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    // always in loop thread
    int nextConnId_{1};          // 下一个连接的 ID
    ConnectionMap connections_;  // 存储所有活动连接的映射
};

}  // namespace muduo

#endif  // MUDUO_NET_TCPSERVER_H
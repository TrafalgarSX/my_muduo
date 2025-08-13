#include "TcpServer.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "SocketsOps.h"

namespace muduo {

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    SPDLOG_DEBUG("Default connection callback: {} -> {}", conn->localAddress().toIpPort(),
                 conn->peerAddress().toIpPort());
    // do not call conn->forceClose(), because some users want to register message callback only.
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer* buf, Timestamp) { buf->retrieveAll(); }

TcpServer::TcpServer(EventLoop* loop, const std::string& name, const InetAddress& listenAddr, Option option)
    : loop_(loop),
      ipPort_(listenAddr.toIpPort()),
      name_(name),
      acceptor_(new Acceptor(loop, listenAddr, static_cast<int>(option))),
      threadPool_(std::make_shared<EventLoopThreadPool>(loop, name)),
      connectionCallback_(defaultConnectionCallback),
      messageCallback_(defaultMessageCallback)
{
    acceptor_->setNewConnectionCallback(
        [this](Socket socket, const InetAddress& peerAddr) { newConnection(std::move(socket), peerAddr); });
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();

    for (auto& item : connections_) {
        TcpConnectionPtr conn(item.second);
        // 减少引用计数，触发连接的析构函数
        // 不调用 erase 是因为要让连接在对应的 EventLoop 中被删除
        item.second.reset();
        conn->getLoop()->runInLoop([this, conn]() { removeConnection(conn); });
    }
    SPDLOG_DEBUG("TcpServer {} destroyed", name_);
}

void TcpServer::setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }

void TcpServer::setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }

void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}


std::shared_ptr<EventLoopThreadPool> TcpServer::threadPool()
{
  return threadPool_;    
}

void TcpServer::start()
{
  if (started_ == 0)
  {
    threadPool_->start(threadInitCallback_);
    assert(!acceptor_->listening());

    loop_->runInLoop([acceptor = acceptor_.get()](){
        acceptor->listen();
    });
    started_ = 1;
  }
}

void TcpServer::newConnection(Socket socket, const InetAddress& peerAddr)
{
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    SPDLOG_DEBUG("New connection from {}", peerAddr);

    // 创建新的连接对象
    std::string connName = fmt::format("{}-{}#{}", name_, ipPort_, nextConnId_++);

    InetAddress localAddr(sockets::getLocalAddr(socket.fd()));
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, std::move(socket), localAddr, peerAddr);

    connections_[connName] = conn;

    // 设置回调函数
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback([this, conn](const TcpConnectionPtr&){
        removeConnection(conn);
    });

  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop([this, conn](){
    removeConnectionInLoop(conn);
  });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop([this, conn](){
    conn->connectDestroyed();  // 调用连接销毁回调
  });
}

}  // namespace muduo
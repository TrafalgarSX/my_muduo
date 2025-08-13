#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <base/noncopyable.h>
#include <functional>

#include "Channel.h"
#include "InetAddress.h"
#include "Socket.h"

namespace muduo {
    
class EventLoop;

class Acceptor: public noncopyable{
public:
  using NewConnectionCallback = std::function<void (Socket, const InetAddress&)>;
  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();
  
  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }
  
  void listen();

  bool listening() const { return listening_; }

  private:
  void handleRead();

  EventLoop* loop_{nullptr};
  Socket acceptSocket_;
  Channel acceptChannel_;
  bool listening_{false};
  NewConnectionCallback newConnectionCallback_;
  int idleFd_;
};

}

#endif // MUDUO_NET_ACCEPTOR_H
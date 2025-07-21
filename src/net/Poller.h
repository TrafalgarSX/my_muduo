#ifndef MUDUO_NET_POLLER_H
#define MUDUO_NET_POLLER_H

#include <base/Timestamp.h>
#include <base/noncopyable.h>

#include <unordered_map>
#include <vector>

namespace muduo {

class EventLoop;
class Channel;

/*
    Poller是EventLoop的间接成员(std::unque_PTR)，只供其owner EventLoop在IO线程调用，因此无须加锁。其生命期与EventLoop相等。
    Poller并不拥有Channel，Channel在析构之前必须自己unregister（EventLoop::removeChannel()），避免空悬指针。

    This class doesn't own the Channel objects.
*/

class Poller : public noncopyable
{
   public:
    using ChannelArray = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller();

    // poll the I/O events
    // Must be called in the loop thread.
    virtual Timestamp poll(int timeoutMs, ChannelArray& activeChannels) = 0;

    // Changes the interested I/O events.
    // Must be called in the loop thread.
    virtual void updateChannel(Channel* channel) = 0;
    // Remove the channel, when it destructs.
    // Must be called in the loop thread.
    virtual void removeChannel(Channel* channel) = 0;
    virtual bool hasChannel(Channel* channel) const;

    void assertInLoopThread() const;

    static Poller* newDefaultPoller(EventLoop* loop);

   protected:
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;  // fd -> Channel mapping, does not own the channels

   private:
    EventLoop* ownerLoop_; // IO thread that owns this Poller， Poller not owns the EventLoop
};

}  // namespace muduo

#endif  // MUDUO_NET_POLLER_H
#ifndef MUDUO_NET_EPOLLPOLLER_H
#define MUDUO_NET_EPOLLPOLLER_H

#include <base/Timestamp.h>

#include <vector>

#include "../Poller.h"

namespace muduo {

class EventLoop;
class Channel;

class EPollPoller : public Poller
{
   public:
    EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList& activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

   private:
    static const int kInitEventListSize = 16;
    static const char* operationToString(int op);
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);

    using EventArray = std::vector<struct epoll_event>;
    int epollfd_;        // epoll file descriptor
    EventArray events_;  // epoll events
};

}  // namespace muduo

#endif  // MUDUO_NET_EPOLLPOLLER_H
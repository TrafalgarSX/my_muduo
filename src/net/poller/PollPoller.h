#ifndef MUDUO_NET_POLLPOLLER_H
#define MUDUO_NET_POLLPOLLER_H

#include <base/Timestamp.h>

#include <vector>

#include "../Poller.h"

struct pollfd;

namespace muduo {

class EventLoop;
class Channel;

class PollPoller : public Poller
{
   public:
    PollPoller(EventLoop* loop);
    ~PollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList& activeChannels) override;

    // 负责维护和更新pollfds_数组
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

   private:
    void fillActiveChannels(int numEvents, ChannelList& activeChannels) const;

    using PollFdArray = std::vector<struct pollfd>;
    PollFdArray pollfds_;
};

}  // namespace muduo

#endif  // MUDUO_NET_POLLPOLLER_H
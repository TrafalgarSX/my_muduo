#include "Poller.h"

#include "Channel.h"
#include "EventLoop.h"
#include "poller/EPollPoller.h"
#include "poller/PollPoller.h"

namespace muduo {

Poller::Poller(EventLoop* loop) : ownerLoop_(loop) {}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const
{
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

void Poller::assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }

#define MUDUO_USE_POLL

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
#ifdef MUDUO_USE_POLL
    SPDLOG_INFO("Using PollPoller");
    return new PollPoller(loop);
#else
    SPDLOG_INFO("Using EPollPoller");
    return new EPollPoller(loop);
#endif
}

}  // namespace muduo
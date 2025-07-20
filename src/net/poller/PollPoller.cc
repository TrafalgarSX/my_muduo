#include "PollPoller.h"

#include <spdlog/spdlog.h>
#include <sys/poll.h>

#include <cerrno>
#include <cstring>

#include "../Channel.h"

namespace muduo {

PollPoller::PollPoller(EventLoop* loop) : Poller(loop)
{
    SPDLOG_DEBUG("PollPoller created: {}", static_cast<void*>(this));
}

PollPoller::~PollPoller() { SPDLOG_DEBUG("PollPoller destroyed: {}", static_cast<void*>(this)); }

Timestamp PollPoller::poll(int timeoutMs, ChannelList& activeChannels)
{
    int numEvents = ::poll(pollfds_.data(), static_cast<nfds_t>(pollfds_.size()), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        SPDLOG_DEBUG("PollPoller::poll - {} events", numEvents);
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        SPDLOG_DEBUG("PollPoller::poll - no events");
    } else {
        if (savedErrno != EINTR) {
            errno = savedErrno;  // Restore errno
            SPDLOG_ERROR("PollPoller::poll - error: {}", strerror(savedErrno));
        } else {
            SPDLOG_DEBUG("PollPoller::poll - interrupted by signal");
        }
    }
    return now;
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList& activeChannels) const
{
    for (const auto& pfd : pollfds_) {
        if (numEvents <= 0) {
            break;  // No more events to process
        }

        if (pfd.revents > 0) {
            --numEvents;
            auto it = channels_.find(pfd.fd);
            if (it != channels_.end()) {
                Channel* channel = it->second;
                channel->setRevents(pfd.revents);
                // pfd->revents = 0; // TODO Reset revents if needed, where it is appropriate?
                activeChannels.push_back(channel);
            } else {
                SPDLOG_ERROR("PollPoller::fillActiveChannels - fd {} not found in channels", pfd.fd);
            }
        }
    }
}

// 负责维护和更新pollfds_数组
void PollPoller::updateChannel(Channel* channel)
{
    assertInLoopThread();
    SPDLOG_DEBUG("fd = {}, events = {}", channel->fd(), channel->events());
    if (channel->index() < 0) {
        // a new one, add to pollfds_
        struct pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;  // Initialize revents to 0
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;
        channel->set_index(idx);
        channels_[channel->fd()] = channel;
    } else {
        /*
        如果某个Channel暂时不关心任何事件，就把pollfd.fd设为-1，让poll(2)忽略此项。
        这里不能改为把pollfd.events设为0，这样无法屏蔽POLLER事件。
        改进的做法是把pollfd.fd设为channel->fd()的相反数减一，这样可以进一步检查invariant。（思考：为什么要减一？）
        */
        int idx = channel->index();
        struct pollfd& pfd = pollfds_[idx];
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;  // Reset revents to 0
        // TODO Why? I can't understand this logic
        if (channel->isNoneEvent()) {
            // ignore this pollfd
            pfd.fd = -channel->fd() - 1;
        }
    }
}

void PollPoller::removeChannel(Channel* channel) {}

}  // namespace muduo
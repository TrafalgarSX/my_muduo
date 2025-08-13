#include "EPollPoller.h"

#include <base/LogInit.h>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <unistd.h>

#include "../Channel.h"

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
static_assert(EPOLLIN == POLLIN,        "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI,      "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT,      "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP,  "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR,      "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP,      "epoll uses same flag values as poll");

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}  // namespace

namespace muduo {

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0) {
        SPDLOG_ERROR("Failed to create epoll fd: {}", strerror(errno));
        abort();
    }
    SPDLOG_DEBUG("EPollPoller created: {}", static_cast<void*>(this));
}

EPollPoller::~EPollPoller()
{
    SPDLOG_DEBUG("EPollPoller destroyed: {}", static_cast<void*>(this));
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelArray& activeChannels)
{
    int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0) {
        SPDLOG_DEBUG("{} events", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        SPDLOG_DEBUG("no events");
    } else {
        if (savedErrno != EINTR) {
            errno = savedErrno;  // Restore errno
            SPDLOG_ERROR("error: {}", strerror(savedErrno));
        } else {
            SPDLOG_DEBUG("interrupted by signal");
        }
    }
    return now;
}

/*
   this will fill activeChannels with the channels that have events
   it iterates through pollfds_ and checks revents for each fd
   if revents is non-zero, it means the channel has events to handle
*/
void EPollPoller::fillActiveChannels(int numEvents, ChannelArray& activeChannels) const
{
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels.push_back(channel);
    }
}

void EPollPoller::updateChannel(Channel* channel)
{
    assertInLoopThread();
    SPDLOG_DEBUG("fd = {}, events = {}", channel->fd(), channel->events());
    const int index = channel->index();
    if (index == kNew || index == kDeleted) {
        // a new one, add to pollfds_
        int fd = channel->fd();

        if (index == kNew) {
            assert(channels_.find(fd) == channels_.end());
            // Add the channel in the map
            channels_[fd] = channel;
        } else {
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    event.events = channel->events();
    event.data.ptr = channel;  // Store the channel pointer in the event data
    if (::epoll_ctl(epollfd_, operation, channel->fd(), &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            SPDLOG_ERROR("epoll_ctl op = {}, fd = {} failed: {}", operationToString(operation), channel->fd(),
                         strerror(errno));
        } else {
            SPDLOG_FATAL("epoll_ctl op = {}, fd = {} failed: {}", operationToString(operation), channel->fd(),
                         strerror(errno));
        }
    }
}

void EPollPoller::removeChannel(Channel* channel)
{
    assertInLoopThread();
    assert(channel->isNoneEvent());

    int index = channel->index();
    assert(index == kAdded || index == kDeleted);

    size_t n = channels_.erase(channel->fd());
    assert(n == 1);
    (void)n;

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

const char* EPollPoller::operationToString(int op)
{
  switch (op)
  {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_DEL:
      return "DEL";
    case EPOLL_CTL_MOD:
      return "MOD";
    default:
      assert(false && "ERROR op");
      return "Unknown Operation";
  }
}

}  // namespace muduo
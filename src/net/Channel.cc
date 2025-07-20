
#include "Channel.h"

#include <poll.h>

#include "EventLoop.h"

namespace muduo {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(kNoneEvent), revents_(kNoneEvent), index_(-1)
{
}

Channel::~Channel() {}

/*
    POLLNVAL: 无效的请求，表示fd无效
    POLLHUP: 挂起事件，表示连接被关闭
    POLLERR: 错误事件，表示发生了错误
    POLLIN: 可读事件，表示有数据可读
    POLLPRI: 优先级数据可读事件，表示有紧急数据可读
    POLLOUT: 可写事件，表示可以写数据
*/
void Channel::handleEvent(Timestamp receiveTime)
{
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (logHup_) {
            SPDLOG_WARN("Channel::handle_event - fd = {}, POLLHUP", fd_);
        }
        if (closeCallback_) closeCallback_();
    }
    if (revents_ & POLLNVAL) {
        SPDLOG_ERROR("Channel::handle_event - POLLNVAL");
    }
    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & POLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}

void Channel::update()
{
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove()
{
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

}  // namespace muduo
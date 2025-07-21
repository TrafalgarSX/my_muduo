#ifndef MUDUO_NET_POLLPOLLER_H
#define MUDUO_NET_POLLPOLLER_H

#include <base/Timestamp.h>

#include <vector>

#include "../Poller.h"

struct pollfd;

namespace muduo {

class EventLoop;
class Channel;

/*
  ChannelMap channels_ 继承自 Poller 类，其他有 Channel 对象所有权的类创建 Channel 对象，并在 Poller::updateChannel() 中注册。
  注册的时候会创建一个 pollfd 结构体，并将其添加到 pollfds_ 数组中。每个 Channel 对象用成员 idx 来记录在 pollfds_ 数组中的索引。
  Poller::updateChannel() 会根据 Channel 的状态来更新 pollfds_ 数组, 主要是更新 pollfds_ 数组对对应 fd 监控的事件。
  
  poll 函数会遍历 pollfds_ 数组，检查每个 fd 的事件，并将有事件的 fd 需要响应的事件设置到对应 Channel 中（通过 fd 在 channels_
  中查找对应的 Channel）, 然后将这些 channel 对象添加到 activeChannels 数组中（出参）。
  
  Channel 对象的事件处理函数会在 EventLoop::loop() 中被调用。对象析构的时候会调用 Poller::removeChannel() 来从 pollfds_ 数组中移除对应的 fd。
  
  Channel 类的 disableAll() 成员函数会将 Channel 的事件设置为 kNoneEvent，并调用 Poller::updateChannel() 来更新 pollfds_ 数组中
  对应的 pollfd 结构体，将其 fd 设置为 -fd-1，这样 poll 函数就会忽略这个 fd(可以恢复）。
*/
class PollPoller : public Poller
{
   public:
    PollPoller(EventLoop* loop);
    ~PollPoller() override;

    Timestamp poll(int timeoutMs, ChannelArray& activeChannels) override;

    // 负责维护和更新pollfds_数组
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

   private:
    void fillActiveChannels(int numEvents, ChannelArray& activeChannels) const;

    using PollFdArray = std::vector<struct pollfd>;
    PollFdArray pollfds_;
};

}  // namespace muduo

#endif  // MUDU
#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <base/Timestamp.h>
#include <base/noncopyable.h>
#include <date/date.h>

#include <functional>

namespace muduo {

class EventLoop;

/*
    每个Channel对象自始至终只属于一个EventLoop，因此每个Channel对象都只属于某一个IO线程。
    每个Channel对象自始至终只负责一个文件描述符（fd）的IO事件分发，但它并不拥有这个fd，也不会在析构的时候关闭这个fd。
    Channel会把不同的IO事件分发为不同的回调，例如ReadCallback、WriteCallback等，而且“回调”用std::function表示，用户
    无须继承Channel，Channel不是基类。
    muduo用户一般不直接使用Channel，而会使用更上层的封装，如TcpConnection。
    Channel的生命期由其owner class负责管理，它一般是其他class的直接或间接成员。
    Channel的成员函数都只能在IO线程调用，因此更新数据成员都不必加锁。

    This class doesn't own the file descriptor.
    The file descriptor could be a socket, an eventfd, a timerfd, or a signalfd
*/
class Channel : public noncopyable
{
   public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    /// Tie this channel to the owner object managed by shared_ptr,
    /// prevent the owner object being destroyed in handleEvent.
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }
    void setRevents(int revt) { revents_ = revt; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }

    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for debug
    std::string reventsToString() const;
    std::string eventsToString() const;
    void doNotLogHup() { logHup_ = false; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();

   private:
    static std::string eventsToString(int fd, int ev);
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

   private:
    EventLoop* loop_;
    const int fd_;
    int events_;   // events to be monitored
    int revents_;  // events that are triggered, set by Poller
    int index_;    // used by Poller
    bool logHup_{true};

    std::weak_ptr<void> tie_;
    bool tied_;
    bool eventHandling_;
    bool addedToLoop_{false};

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

}  // namespace muduo

#endif  // MUDUO_NET_CHANNEL_H

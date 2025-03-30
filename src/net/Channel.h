#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <base/noncopyable.h>

#include <date/date.h>
#include <functional>

class EventLoop;

namespace muduo {
class Channel : public noncopyable
{
   public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void()>;
    Channel(EventLoop* loop, int fd);

    void handleEvent();
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }

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

    // for Poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* ownerLoop() { return loop_; }

   private:
    void update();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

   private:
    EventLoop* loop_;
    const int fd_;
    int events_;
    int revents_;
    int index_; // used by Poller

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};

}  // namespace muduo

#endif

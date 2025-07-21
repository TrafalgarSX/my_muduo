#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <base/CurrentThread.h>
#include <base/Timestamp.h>
#include <base/noncopyable.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <any>
#include <functional>
#include <memory>
#include <mutex>

#include "Callbacks.h"
#include "TimerId.h"

namespace muduo {

class Channel;
class Poller;
class TimerQueue;

/*
    创建了EventLoop对象的线程是IO线程
    EventLoop对象的生命期通常和其所属的线程一样长，它不必是heap对象
*/
class EventLoop : noncopyable
{
   public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();

    void quit();

    // other thread can call this to wake up the loop
    /*
    Runs callback immediately in the loop thread.
    It wakes up the loop, and run the cb.
    If in the same loop thread, cb is run within the function.
    Safe to call from other threads.
    */
    void runInLoop(Functor cb);
    /*
    Queues callback in the loop thread.
    Runs after finish pooling.
    Safe to call from other threads.
    */
    void wakeup();
    void queueInLoop(Functor cb);
    size_t queueSize() const;

    // timers
    /*
      Runs callback at 'time'.
      Safe to call from other threads.
    */
    TimerId runAt(Timestamp time, TimerCallback cb);
    /*
      Runs callback after @c delay seconds.
      Safe to call from other threads.
    */
    TimerId runAfter(double delay, TimerCallback cb);

    /*
      Runs callback every @c interval seconds.
      Safe to call from other threads.
    */
    TimerId runEvery(double interval, TimerCallback cb);
    /*
      Cancels the timer.
      Safe to call from other threads.
    */
    void cancel(TimerId timerId);

    // internal usage
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // check if the current thread is the IO thread
    void assertInLoopThread() const;
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    static EventLoop* getEventLoopOfCurrentThread();

   private:
    void abortNotInLoopThread() const;
    void handleRead(); // 处理wakeupFd_上的读事件，唤醒EventLoop的loop()函数
    void doPendingFunctors(); // 处理待执行的回调函数(大概是从其他线程传递过来的)

   private:
    using ChannelList = std::vector<Channel*>;

    bool looping_{false};
    bool eventHandling_{false};          /* atomic */
    bool callingPendingFunctors_{false}; /* atomic */
    std::atomic<bool> quit_{false};      /* other threads can set this to true to stop the loop */

    const pid_t threadId_;
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_; // Poller is used to poll the I/O events, eventloop owns it
    std::unique_ptr<TimerQueue> timerQueue_; // eventloop owns it, used for scheduling timers

    // for other threads to wake up the loop
    int wakeupFd_;
    // unlike in TimerQueue, which is an internal class,
    // we don't expose Channel to client.
    std::unique_ptr<Channel> wakeupChannel_;
    std::any context_;
    mutable std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;

    // for poller
    ChannelList activeChannels_;  // the list of active channels in the last poll(); does not own the channels
    Channel* currentActiveChannel_{nullptr};
};

}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOP_H
// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <base/Mutex.h>
#include <base/Timestamp.h>

#include <set>
#include <vector>

#include "Callbacks.h"
#include "Channel.h"
#include "EntryCompare.h"

namespace muduo {

class EventLoop;
class Timer;
class TimerId;

/*
  A best efforts timer queue.
  No guarantee that the callback will be on time.
  注意TimerQueue的成员函数只能在其所属的IO线程调用, 因此不必加锁
*/
class TimerQueue : noncopyable
{
   public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    ///
    /// Schedules the callback to be run at given time,
    /// repeats if @c interval > 0.0.
    ///
    /// Must be thread safe. Usually be called from other threads.
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    void cancel(TimerId timerId);

   private:
    // FIXME: use unique_ptr<Timer> instead of raw pointers.
    // This requires heterogeneous comparison lookup (N3465) from C++14
    // so that we can find an T* in a set<unique_ptr<T>>.
    using Entry = std::pair<Timestamp, std::unique_ptr<Timer>>;
    using TimerSet = std::set<Entry, EntryCompareCXX17>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);
    // called when timerfd alarms
    void handleRead();
    // move out all expired timers
    std::vector<Entry> getExpired(Timestamp now);
    void reset(std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_; /* 观察timerfd_上的readable事件 */
    // Timer list sorted by expiration
    TimerSet timers_;

    // for cancel()
    ActiveTimerSet activeTimers_;
    bool callingExpiredTimers_; /* atomic */
    ActiveTimerSet cancelingTimers_;
};

}  // namespace muduo
#endif  // MUDUO_NET_TIMERQUEUE_H

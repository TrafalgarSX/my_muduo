#include "TimerQueue.h"

#include <base/LogInit.h>
#include <spdlog/spdlog.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

namespace muduo {
namespace detail {

int createTimerfd()
{
    // 此时 timerfd 不会触发任何事件
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        SPDLOG_FATAL("Failed to create timerfd: {}", strerror(errno));
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100) {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    SPDLOG_TRACE("TimerQueue::readTimerfd() {} at {}", howmany, now.toString());
    if (n != sizeof howmany) {
        SPDLOG_ERROR("TimerQueue::readTimerfd() reads {} bytes instead of 8", n);
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    std::memset(&newValue, 0, sizeof newValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, nullptr);
    if (ret) {
        SPDLOG_ERROR("timerfd_settime()");
    }
}

}  // namespace detail

}  // namespace muduo

namespace muduo {

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(detail::createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback([this](Timestamp) { handleRead(); });
    // we are always reading the timerfd, we disarm it with timerfd_settime.
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop([this, timer]() { addTimerInLoop(timer); });
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(std::unique_ptr<Timer>(timer));

    if (earliestChanged) {
        detail::resetTimerfd(timerfd_, timer->expiration());
    }
}

bool TimerQueue::insert(std::unique_ptr<Timer> timerPtr)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    Timer* timer = timerPtr.get();
    Timestamp when = timer->expiration();
    auto timersIter = timers_.begin();
    if (timersIter == timers_.end() || when < timersIter->first) {
        earliestChanged = true;
    }

    {
        std::pair<TimerSet::iterator, bool> result = timers_.emplace(when, std::move(timerPtr));
        assert(result.second);
        (void)result;
    }

    {
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.emplace(timer, timer->sequence());
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop([this, timerId]() { cancelInLoop(timerId); });
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer activeTimer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(activeTimer);
    if (it != activeTimers_.end()) {
        // 删除定时器
        Timer* timer = it->first;

        // 这里注意：定时器只能被插入一次，所以可以直接删除
        auto timer_it = std::find_if(timers_.begin(), timers_.end(),
                                     [timer](const Entry& entry) { return entry.second.get() == timer; });

        if (timer_it != timers_.end()) {
            timers_.erase(timer_it);
        }

        activeTimers_.erase(it);
    } else if (callingExpiredTimers_) {
        // 在定时器回调函数中取消其他定时器时会触发此分支
        cancelingTimers_.insert(activeTimer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    detail::readTimerfd(timerfd_);

    // std::vector<Entry> expired = std::move(getExpired(now));
    Timestamp now(Timestamp::now());
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    // TODO why cancelingTimers_ is cleared here?
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (const Entry& it : expired) {
        it.second->run();  // could be call cancel() inside
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    TimerSet::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);

    std::vector<Entry> expired;
    // 预分配空间，提高性能
    size_t count = std::distance(timers_.begin(), end);
    expired.reserve(count);

    // 使用 extract 从 set 中提取节点并移动到 vector
    for (auto it = timers_.begin(); it != end;) {
        auto node = timers_.extract(it++);              // 提取节点，it 自动前进
        expired.emplace_back(std::move(node.value()));  // 移动节点的值
    }

    for (const Entry& it : expired) {
        ActiveTimer timer(it.second.get(), it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

void TimerQueue::reset(std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (Entry& it : expired) {
        ActiveTimer activeTimer(it.second.get(), it.second->sequence());
        /*
           过时的定时器被处理后，如果它是重复的，并且没有被取消(定时任务中可能会取消其他定时器任务）重新启动它。
        */
        if (it.second->repeat() && cancelingTimers_.find(activeTimer) == cancelingTimers_.end()) {
            it.second->restart(now);
            insert(std::move(it.second));
        }
        // 被取消了，或者不是重复定时器，销毁它, 这里不需要主动调用， expired 中的 it.second 已经是
        // std::unique_ptr<Timer>，会自动调用析构函数释放资源
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        detail::resetTimerfd(timerfd_, nextExpire);
    }
}

}  // namespace muduo
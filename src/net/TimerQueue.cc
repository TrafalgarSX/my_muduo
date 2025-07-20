#include "TimerQueue.h"
#include "EventLoop.h"

#include <spdlog/spdlog.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "Timer.h"
#include "TimerId.h"

namespace muduo {
namespace detail {

int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        SPDLOG_ERROR("Failed to create timerfd: {}", strerror(errno));
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

void readTimerfd(int timerfd, Timestamp now)
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
    struct itimerspec oldValue;
    std::memset(&newValue, 0, sizeof newValue);
    std::memset(&oldValue, 0, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        SPDLOG_ERROR("timerfd_settime()");
    }
}

}  // namespace detail

}  // namespace muduo

namespace muduo {

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop), timerfd_(detail::createTimerfd()), timerfdChannel_(loop, timerfd_), timers_(), callingExpiredTimers_(false)
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

void TimerQueue::cancel(TimerId timerId) 
{ 
    loop_->runInLoop([this, timerId]() { cancelInLoop(timerId); }); 
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);

    if (earliestChanged) {
        detail::resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end()) {
        // TODO 
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        (void)n;
        delete it->first;  // FIXME: no delete please
        activeTimers_.erase(it);
    } else if (callingExpiredTimers_) {
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    detail::readTimerfd(timerfd_, now);

    std::vector<Entry> expired = std::move(getExpired(now));

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (const Entry& it : expired) {
        it.second->run();
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
    for (auto it = timers_.begin(); it != end; ) {
        auto node = timers_.extract(it++);  // 提取节点，it 自动前进
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
        ActiveTimer timer(it.second.get(), it.second->sequence());
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            it.second->restart(now);
            insert(it.second.get());
        } else {
            // FIXME move to a free list
            it.second.reset();
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        detail::resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer* timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerSet::iterator it = timers_.begin();
    if (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }
    {
        std::pair<TimerSet::iterator, bool> result = timers_.insert(Entry(when, std::unique_ptr<Timer>(timer)));
        assert(result.second);
        (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}

}  // namespace muduo
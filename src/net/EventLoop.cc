#include "EventLoop.h"

#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <unistd.h>

#include <mutex>

#include "Channel.h"
#include "Poller.h"
#include "SocketsOps.h"
#include "TimerQueue.h"

namespace {

thread_local muduo::EventLoop* t_loopInThisThread = nullptr;
const int kPollTimeMs = 10000;

int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        SPDLOG_ERROR("Failed to create eventfd: {}", strerror(errno));
        abort();
    }
    return evtfd;
}

}  // namespace

namespace muduo {
EventLoop::EventLoop()
    : threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    SPDLOG_DEBUG("EventLoop created: {}, in thread {}", static_cast<void*>(this), threadId_);
    if (t_loopInThisThread) {
        SPDLOG_ERROR("Another EventLoop {} exists in this thread {}", static_cast<void*>(t_loopInThisThread),
                     threadId_);
    } else {
        t_loopInThisThread = this;
    }
    wakeupChannel_->setReadCallback([this](Timestamp) { handleRead(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    SPDLOG_DEBUG("EventLoop destroyed: {}, in thread {}", static_cast<void*>(this), threadId_);
    t_loopInThisThread = nullptr;
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;
    SPDLOG_DEBUG("EventLoop {} start looping", static_cast<void*>(this));

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, activeChannels_);

        eventHandling_ = true;
        for (Channel* channel : activeChannels_) {
            currentActiveChannel_ = channel;
            channel->handleEvent(pollReturnTime_);  // 在这里处理每个活跃的Channel的事件，可能会调用 removeChannel()
        }
        currentActiveChannel_ = nullptr;
        eventHandling_ = false;
    }
    SPDLOG_DEBUG("EventLoop {} quit", static_cast<void*>(this));
    looping_ = false;
}

/*
  现在可以终止事件循环，只要将quit_设为true即可，但是quit()不是立刻发生的，
  它会在EventLoop::loop()下一次检查while (!quit_)的时候起效。
  如果在非当前IO线程调用quit()，延迟可以长达数秒，将来我们可以唤醒EventLoop以缩小延时。
  但是quit()不是中断或signal，而是设标志，如果EventLoop::loop()正阻塞在某个调用中，quit()不会立刻生效。
*/
void EventLoop::quit()
{
    if (!looping_) {
        SPDLOG_WARN("EventLoop {} is not looping, quit ignored", static_cast<void*>(this));
        return;
    }
    quit_ = true;
    SPDLOG_DEBUG("EventLoop {} quit", static_cast<void*>(this));
    /*
      There is a chance that loop() just executes while(!quit_) and exits,
      then EventLoop destructs, then we are accessing an invalid object.
      Can be fixed using mutex_ in both places.
    */
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();

    // 在事件处理过程中，
    if (eventHandling_) {
        /*
        条件1：currentActiveChannel_ == channel
        正在移除的Channel 就是当前正在处理事件的Channel
        这是安全的，因为Channel可以在自己的事件处理回调中安全地移除自己

        条件2：activeChannels_ 中不包含 channel
        正在移除的Channel 不在当前活跃的Channel列表中
        这也是安全的，因为在事件处理过程中，可能会有其他Channel被移除，
        但我们不会在当前事件处理循环中再次访问它们。
        */
        assert(currentActiveChannel_ == channel ||
               std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::assertInLoopThread() const
{
    if (!isInLoopThread()) {
        abortNotInLoopThread();
    }
}

/*
    返回值可能为 nullptr，如果当前线程不是 IO 线程的话
*/
EventLoop* EventLoop::getEventLoopOfCurrentThread() { return t_loopInThisThread; }

/*
   下列的函数是EventLoop的公共接口，允许其他线程向EventLoop发送任务。
   这些函数可以在任何线程中调用，但它们会将任务排入EventLoop的待处理任务队列中，
   并在EventLoop的IO线程中执行。
   这使得EventLoop可以安全地处理来自其他线程的任务，而不会引起线程安全问题。
*/
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        SPDLOG_ERROR("EventLoop::wakeup() writes {} bytes instead of 8", n);
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

size_t EventLoop::queueSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingFunctors_.size();
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

/*
    处理wakeupFd_上的读事件，唤醒EventLoop的loop()函数。
    这个函数会在EventLoop::loop()中调用，通常是通过wakeup()触发的。

    这个函数就是协助非 IO 线程唤醒 EventLoop 的 loop() 函数，
*/
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        SPDLOG_ERROR("EventLoop::handleRead() reads {} bytes instead of 8", n);
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

// timers
/*
    在EventLoop中添加一个定时器，定时器将在指定的时间点执行回调函数。
    可能是其他线程传递过来的, 因为addTimer() 内部是调用的EventLoop::runInLoop()。
    如果interval大于0.0，则定时器会重复执行。
*/
TimerId EventLoop::runAt(Timestamp time, TimerCallback cb) { return timerQueue_->addTimer(std::move(cb), time, 0.0); }

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId) { return timerQueue_->cancel(timerId); }

void EventLoop::abortNotInLoopThread() const
{
    SPDLOG_ERROR("EventLoop was created in threadId = {}, current thread id = {}", threadId_, CurrentThread::tid());
    // abort(); // TODO muduo 没有真的 abort， why?
}

}  // namespace muduo

#include "EventLoop.h"
#include <spdlog/spdlog.h>

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
namespace muduo{
thread_local EventLoop* t_loopInThisThread = 0;

EventLoop::EventLoop()
    :looping_(false),
     threadId_(CurrentThread::tid())
{
    SPDLOG_INFO("EventLoop created: {}, in thread {}", this, threadId_);
    if(t_loopInThisThread)
    {
        SPDLOG_ERROR("Another EventLoop {} exists in this thread {}", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
}

EventLoop::~EventLoop()
{
    assert(!looping_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    ::poll(NULL, 0, 5 * 1000);
    SPDLOG_INFO("EventLoop {} start looping", this);
    looping_ = false;

}

void EventLoop::quit()
{

}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;
}

}

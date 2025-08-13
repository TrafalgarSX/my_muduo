#include "EventLoopThreadPool.h"

#include <string>
#include <vector>
#include <memory>

#include "EventLoop.h"
#include "EventLoopThread.h"

namespace muduo {

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop), name_(nameArg) 
{
}

EventLoopThreadPool::~EventLoopThreadPool() 
{
    SPDLOG_DEBUG("EventLoopThreadPool destroyed: {}, in thread {}", static_cast<void*>(this), baseLoop_->threadId());
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) 
{
  baseLoop_->assertInLoopThread();
  
  started_ = true;
  
  for (int i = 0; i < numThreads_; ++i) {
      std::unique_ptr<EventLoopThread> eventLoopThread = std::make_unique<EventLoopThread>(cb);
      eventLoopThreads_.emplace_back(std::move(eventLoopThread));
      loops_.push_back(eventLoopThread->startLoop());
  }

  if (numThreads_ == 0 && cb)
  {
    cb(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  EventLoop* loop = baseLoop_;

  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_];
    ++next_;
    if (static_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
}

EventLoop* EventLoopThreadPool::getLoopForHash(size_t hashCode)
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_;

  if (!loops_.empty())
  {
    loop = loops_[hashCode % loops_.size()];
  }
  return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops()
{
  baseLoop_->assertInLoopThread();
  assert(started_);
  // TcpServer 中 setThreadNum 为 0 的情况： all I/O in loop's thread, no thread will created.
  if (loops_.empty())
  {
    return std::vector<EventLoop*>(1, baseLoop_);
  }
  else
  {
    return loops_;
  }
}



}  // namespace muduo
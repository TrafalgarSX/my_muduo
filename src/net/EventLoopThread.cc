#include "EventLoopThread.h"

#include "EventLoop.h"

namespace muduo {

EventLoopThread::EventLoopThread(ThreadInitCallback cb): callback_(std::move(cb))
{
    SPDLOG_DEBUG("EventLoopThread created: {}", static_cast<void*>(this));
}

EventLoopThread::~EventLoopThread()
{
    if(loop_) {
        loop_->quit(); // Ensure the loop is stopped before destroying
    }
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    if(thread_) {
        SPDLOG_ERROR("EventLoopThread already started");
        return loop_;
    }

    thread_ = std::make_unique<std::thread>(&EventLoopThread::threadFunc, this);

    // Wait until the EventLoop is created and ready
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return loop_ != nullptr; });// Wait for the loop to be initialized
    }

    return loop_;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_all();  // Notify that the loop is ready
    }

    loop.loop();
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}

}  // namespace muduo
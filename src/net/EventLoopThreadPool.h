#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <base/noncopyable.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace muduo {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable
{
   public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // valid after calling start()
    /// round-robin
    EventLoop* getNextLoop();

    /// with the same hash code, it will always return the same EventLoop
    EventLoop* getLoopForHash(size_t hashCode);

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }

    const std::string& name() const { return name_; }

   private:
    EventLoop* baseLoop_;
    std::string name_;
    bool started_{false};
    int numThreads_{0};
    int next_{0};
    std::vector<std::unique_ptr<EventLoopThread>> eventLoopThreads_;
    std::vector<EventLoop*> loops_;
};

}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
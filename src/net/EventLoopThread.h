#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <base/noncopyable.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace muduo {

class EventLoop;

using ThreadInitCallback = std::function<void(EventLoop*)>;

class EventLoop;
class EventLoopThread : noncopyable
{
   public:
    EventLoopThread(ThreadInitCallback cb = nullptr);
    ~EventLoopThread();

    // start the thread and return the EventLoop
    EventLoop* startLoop();

   private:
    void threadFunc();  // 线程函数，创建EventLoop并开始loop

   private:
    EventLoop* loop_{nullptr};                      // 线程内的EventLoop
    std::unique_ptr<std::thread> thread_{nullptr};  // 线程对象
    std::mutex mutex_;                              // 互斥锁
    std::condition_variable cond_;                  // 条件变量，用于线程同步
    ThreadInitCallback callback_;  // 线程初始化回调函数，可能用于设置一些初始状态或配置
};

}  // namespace muduo

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H
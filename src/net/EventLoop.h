#include <base/noncopyable.h>
#include <base/CurrentThread.h>

#include <functional>
#include <cstdint>

namespace muduo 
{
class EventLoop : noncopyable
{
public:
    typedef std::function<void> Functor;

    EventLoop();
    ~EventLoop();

    void loop();

    void quit();

    void assertInLoopThread() const
    {
        if(!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }

    bool isInLoopThread() const
    {
        return threadId_ == CurrentThread::tid();
    }


private:
    void abortNotInLoopThread() const
    {
        SPDLOG_ERROR("EventLoop::abortNotInLoopThread - EventLoop was created in threadId = {}, current thread id = {}",
                     threadId_, CurrentThread::tid());
        abort();
    }
    bool looping_;
    const pid_t threadId_;
};

}
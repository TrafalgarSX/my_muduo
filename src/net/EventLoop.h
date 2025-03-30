#include <base/CurrentThread.h>
#include <base/noncopyable.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <functional>

namespace muduo {
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
        if (!isInLoopThread()) {
            abortNotInLoopThread();
        }
    }

    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

    static EventLoop* getEventLoopOfCurrentThread();

   private:
    void abortNotInLoopThread() const
    {
        SPDLOG_ERROR("EventLoop::abortNotInLoopThread - EventLoop was created in threadId = {}, current thread id = {}",
                     threadId_, CurrentThread::tid());
        abort();
    }

   private:
    bool looping_;
    const pid_t threadId_;
};

}  // namespace muduo
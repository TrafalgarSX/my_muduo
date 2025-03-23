#include "Thread.h"

#include "CurrentThread.h"
#include "assert.h"

#include <spdlog/spdlog.h>

namespace muduo {

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false), joined_(false), tid_(0), func_(std::move(func)), name_(name), latch_(1)  // 计数器初始值为1
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_) {
        thread_->detach();  // thread类提供了设置分离线程的方法 线程运行后自动销毁（非阻塞）
    }
}

void Thread::start()  // 一个Thread对象 记录的就是一个新线程的详细信息
{
    assert(!started_);
    started_ = true;
    // 开启线程
    thread_ = std::shared_ptr<std::thread>(new std::thread([this]() {
        tid_ = CurrentThread::tid();  // 获取线程的tid值
        latch_.countDown();           // 计数器减1
        muduo::CurrentThread::t_threadName = name_.empty() ? "muduoThread" : name_;  // 设置线程名称
        try{
            func_();                      // 开启一个新线程 专门执行该线程函数
            muduo::CurrentThread::t_threadName = "finished";
        }catch(const std::exception& ex){
            muduo::CurrentThread::t_threadName = "crashed";
            SPDLOG_ERROR("exception caught in Thread: %s", ex.what());
            SPDLOG_ERROR("Thread exception: %s", ex.what());
            abort();  // 线程函数执行异常，终止进程
        }

    }));

    latch_.wait();  // 等待线程函数执行完毕
    // 这里必须等待获取上面新创建的线程的tid值
}

// C++ std::thread 中join()和detach()的区别：https://blog.nowcoder.net/n/8fcd9bb6e2e94d9596cf0a45c8e5858a
void Thread::join()
{
    assert(started_);
    assert(!joined_);
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if (name_.empty()) {
        name_ = "Thread" + std::to_string(num);  // 线程名称
    }
}
}  // namespace muduo
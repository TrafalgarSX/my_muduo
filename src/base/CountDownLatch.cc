#include "CountDownLatch.h"

#include <mutex>

using namespace muduo;

CountDownLatch::CountDownLatch(int count) : mutex_(), count_(count) {}

void CountDownLatch::wait()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return count_ <= 0; });
}

void CountDownLatch::countDown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (--count_ == 0) {
        cond_.notify_all();
    }
}

int CountDownLatch::getCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

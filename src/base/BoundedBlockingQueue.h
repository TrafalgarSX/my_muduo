// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include <mutex>
#include <condition_variable>

#include <boost/circular_buffer.hpp>
#include <assert.h>
#include "noncopyable.h"

namespace muduo
{

template<typename T>
class BoundedBlockingQueue : noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      queue_(maxSize)
  {
  }

  void put(const T& x)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return !queue_.full(); });
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify_one();
  }

  void put(T&& x)
  {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return !queue_.full(); });
    assert(!queue_.full());
    queue_.push_back(std::move(x));
    notEmpty_.notify_one();
  }

  T take()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return !queue_.empty(); });
    assert(!queue_.empty());
    T front(std::move(queue_.front()));
    queue_.pop_front();
    notFull_.notify_one();
    return front;
  }

  bool empty() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  bool full() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.full();
  }

  size_t size() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  size_t capacity() const
  {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.capacity();
  }

 private:
  mutable std::mutex         mutex_;
  std::condition_variable    notEmpty_;
  std::condition_variable    notFull_;
  boost::circular_buffer<T>  queue_;
};

}  // namespace muduo

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
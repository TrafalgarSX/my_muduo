// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include "CurrentThread.h"
#include "noncopyable.h"
#include <assert.h>
#include <mutex>

namespace muduo
{

class Mutex : noncopyable
{
 public:
  Mutex()
    : holder_(0)
  {
  }

  ~Mutex()
  {
    assert(holder_ == 0);
  }

  // must be called when locked, i.e. for assertion
  bool isLockedByThisThread() const
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked() const 
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock() 
  {
    mutex_.lock();
    assignHolder();
  }

  void unlock() 
  {
    unassignHolder();
    mutex_.unlock();
  }

  std::mutex* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:

  class UnassignGuard : noncopyable
  {
   public:
    explicit UnassignGuard(Mutex& owner)
      : owner_(owner)
    {
      owner_.unassignHolder();
    }

    ~UnassignGuard()
    {
      owner_.assignHolder();
    }

   private:
    Mutex& owner_;
  };

  void unassignHolder()
  {
    holder_ = 0;
  }

  void assignHolder()
  {
    holder_ = CurrentThread::tid();
  }

  std::mutex mutex_;
  pid_t holder_;
};

// Use as a stack variable, eg.
// int Foo::size() const
// {
//   MutexLockGuard lock(mutex_);
//   return data_.size();
// }
class MutexLockGuard : noncopyable
{
 public:
  explicit MutexLockGuard(Mutex& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:
  Mutex& mutex_;
};

}  // namespace muduo

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H

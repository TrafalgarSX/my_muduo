#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <unistd.h>

#include "CountDownLatch.h"
#include "noncopyable.h"

namespace muduo
{

class Thread : noncopyable
{
 public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc, const std::string& name = std::string());
  // FIXME: make it movable in C++11
  ~Thread();

  void start();
  int join(); // return pthread_join()

  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
  pid_t tid() const { return tid_; }
  const std::string& name() const { return name_; }

  static int numCreated() { return numCreated_.load(); }

 private:
  void setDefaultName();

  bool       started_;
  bool       joined_;
  std::thread thread_;
  pid_t tid_;
  ThreadFunc func_;
  std::string     name_;
  CountDownLatch latch_;

  static std::atomic_int32_t  numCreated_;
};

}  // namespace muduo

#endif // MUDUO_BASE_THREAD_H

// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "CurrentThread.h"
#include <unistd.h>
#include <sys/syscall.h>

namespace muduo
{
namespace CurrentThread
{
thread_local int t_cachedTid = 0;
thread_local std::string t_tidString;
static_assert(std::is_same<int, pid_t>::value, "pid_t should be int");

pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

void cacheTid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = gettid();
    t_tidString = std::to_string(t_cachedTid);
  }
}

bool isMainThread()
{
  return tid() == ::getpid();
}


}  // namespace CurrentThread
}  // namespace muduo

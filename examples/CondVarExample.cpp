#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
using namespace std;

mutex mtx;
condition_variable cv;
bool ready = false;

void threadUsingUniqueLock() {
    // 使用 unique_lock 可以在 wait 期间释放锁
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
    cout << "Unique_lock: 条件满足，线程继续执行" << endl;
}

void threadUsingLockGuard() {
    // lock_guard 只负责加锁不能释放锁，故不能用于条件变量等待
    lock_guard<mutex> lock(mtx);
    // 以下伪代码不能通过编译：cv.wait(lock); // 错误：lock 不是 unique_lock
    cout << "Lock_guard: 无法在条件等待中释放互斥量" << endl;
}

int main() {
    thread t(threadUsingUniqueLock);
    // 保证 t 线程进入等待状态
    this_thread::sleep_for(chrono::milliseconds(100));
    {
        lock_guard<mutex> lock(mtx);
        ready = true;
    }
    cv.notify_one();
    t.join();
    cout << "示例结束" << endl;
    return 0;
}

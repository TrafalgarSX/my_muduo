#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <semaphore>

using namespace std;

int main() {
    const int numThreads = 3;
    // 创建一个计数信号量，初始计数为0，最大计数为numThreads
    std::counting_semaphore<numThreads> sem(0);
    
    vector<thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &sem]() {
            cout << "Thread " << i << " is working...\n";
            this_thread::sleep_for(chrono::milliseconds(500 * (i + 1)));
            sem.release();  // 释放信号量，计数增加
            cout << "Thread " << i << " signaled semaphore.\n";
        });
    }
    
    // 主线程等待所有线程信号完成
    for (int i = 0; i < numThreads; ++i) {
        sem.acquire();  // 获取信号量，计数减少
        cout << "Main thread acquired semaphore " << i << ".\n";
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    cout << "All threads finished. Main thread continues.\n";
    return 0;
}
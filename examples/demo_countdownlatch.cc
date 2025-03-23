#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "CountDownLatch.h"

using namespace muduo;

int main() {
    const int numThreads = 3;
    CountDownLatch latch(numThreads);
    std::vector<std::thread> threads;

    // Launch worker threads.
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &latch]() {
            // ...simulate work...
            std::cout << "Thread " << i << " is working...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (i + 1)));
            latch.countDown();
            std::cout << "Thread " << i << " finished work.\n";
        });
    }

    std::cout << "Main thread waiting...\n";
    latch.wait();
    std::cout << "All threads have finished. Main thread continues.\n";

    for(auto& t : threads) {
        t.join();
    }

    return 0;
}

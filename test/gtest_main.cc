#include <base/Thread.h>
#include <gtest/gtest.h>
#include <net/EventLoop.h>
#include <net/Channel.h>
#include <sys/timerfd.h>
#include <fmt/format.h>

void thread_func()
{
    muduo::EventLoop loop;
    loop.loop();
}

TEST(EventLoopTest, DISABLED_NormalEventLoop)
{
    muduo::EventLoop loop;
    ASSERT_TRUE(loop.isInLoopThread());

    muduo::Thread thread(thread_func);
    thread.start();

    loop.loop();
}

muduo::EventLoop* g_loop = nullptr;

void thread_func2()
{
    g_loop = muduo::EventLoop::getEventLoopOfCurrentThread();
    ASSERT_TRUE(g_loop != nullptr);
    g_loop->loop();
}

TEST(EventLoopTest, DISABLED_WrongEventLoop)
{
    muduo::EventLoop loop;
    g_loop = &loop;

    muduo::Thread thread(thread_func2);
    thread.start();

    // 使用 EXPECT_DEATH 来捕获 abort 调用
    EXPECT_DEATH({
        thread.join();
    }, "EventLoop::abortNotInLoopThread.*");  // 匹配错误信息的正则表达
}


TEST(EventLoopTest, TimeoutEventLoop)
{
    muduo::EventLoop loop;
    ASSERT_TRUE(loop.isInLoopThread());
    
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    muduo::Channel timerChannel(&loop, timerfd);
    timerChannel.setReadCallback([&loop](muduo::Timestamp receiveTime) {
        fmt::print("Timer triggered at {}\n", receiveTime.toString());
        loop.quit();
    });
    
    timerChannel.enableReading();
    
    struct itimerspec how_long;
    bzero(&how_long, sizeof how_long);
    how_long.it_value.tv_sec = 2;  // 2 seconds
    ::timerfd_settime(timerfd, 0, &how_long, nullptr);

    // 运行事件循环，等待定时器触发
    loop.loop();
    
    ::close(timerfd);
}


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    
        // 排除特定测试
    // ::testing::GTEST_FLAG(filter) = "-EventLoopTest.WrongEventLoop,-EventLoopTest.NormalEventLoop";
   
    return RUN_ALL_TESTS();
}
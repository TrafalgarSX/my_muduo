#ifndef MUDUO_BASE_LOGINIT_H
#define MUDUO_BASE_LOGINIT_H

void initLog();
void shutodwnLog();

// 添加 FATAL 级别的日志宏
#define SPDLOG_FATAL(...) \
    do { \
        SPDLOG_CRITICAL("FATAL: " __VA_ARGS__); \
        spdlog::shutdown(); \
        std::abort(); \
    } while(0)

#endif // MUDUO_BASE_LOGINIT_H
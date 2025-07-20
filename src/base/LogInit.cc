#include "LogInit.h"

#include <filesystem>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace {
    
std::string get_app_data_directory()
{
    std::filesystem::path appDataPath;
    // cpp 17 filesystem
#ifdef __linux__
    // 如果没有设置 XDG_DATA_HOME，使用 ~/.local/share
    const char* home = std::getenv("HOME");
    if (home) {
        appDataPath = std::filesystem::path(home) / ".local" / "share" / "muduo";
    } else {
        // 最后的备选方案，使用临时目录
        appDataPath = std::filesystem::temp_directory_path() / "muduo_logs";
    }
#elif defined(_WIN32) || defined(_WIN64)
    // Windows 下使用 AppData 路径
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        appDataPath = std::filesystem::path(appData) / "muduo";
    } else {
        // 最后的备选方案，使用临时目录
        appDataPath = std::filesystem::temp_directory_path() / "muduo_logs";
    }
#endif
    
    if (!std::filesystem::exists(appDataPath)) {
        std::filesystem::create_directories(appDataPath);
    }
    return appDataPath.string();
}

} // namespace anonymous

void initLog()
{
    // 初始化线程池，队列大小为8192，线程数量为1

    spdlog::init_thread_pool(8192, 1);

    std::string logDir = get_app_data_directory();

    std::string logPathStr = logDir + "/" + "EsLogX.txt";
    // 创建 rotating file sink
    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathStr, 1024 * 1024 * 50, 3);
    rotating_sink->set_level(spdlog::level::debug);

#if NDEBUG
    std::vector<spdlog::sink_ptr> sinks{rotating_sink};
#else

    // 创建控制台 sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    // 将多个 sink 组合到一个 logger 中
    std::vector<spdlog::sink_ptr> sinks{console_sink, rotating_sink};

#endif

    // 创建异步日志器
    auto logger = std::make_shared<spdlog::async_logger>("multi_sink", sinks.begin(), sinks.end(),
                                                         spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    // 创建同步日志器
    // auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
    spdlog::register_logger(logger);

    // 设置 spdlog 格式
    // spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%l][%s:%# %!] %v");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%l]%v");
    // 设置全局日志级别
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
    spdlog::flush_every(std::chrono::seconds(1));
    spdlog::info("[EsLogInit.cpp:131] etmc log started");
}

void shutodwnLog()
{
    spdlog::shutdown();
    spdlog::drop_all();
}
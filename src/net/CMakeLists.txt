cmake_minimum_required(VERSION 3.22)

project(muduo_net)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND muduo_net_libs
    fmt::fmt
    spdlog::spdlog
    date::date date::date-tz
)

list(APPEND muduo_net_src
    EventLoop.cc
    EventLoop.h
)

# 修改此处，使用 muduo_net_src 而非 muduo_base_src
add_library(muduo_net ${muduo_net_src})
target_include_directories(muduo_net
	PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

# 使用变量展开
target_link_libraries(muduo_net
    PUBLIC
        ${muduo_net_libs}
)

target_sources(muduo_net
    PRIVATE
        ${muduo_net_src}
)
cmake_minimum_required(VERSION 3.22)

project(muduo_base)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

list(APPEND muduo_base_libs
    fmt::fmt
    spdlog::spdlog
    date::date date::date-tz
)

list(APPEND muduo_base_src
    BlockingQueue.h
    BoundedBlockingQueue.h
	CountDownLatch.cc
	CountDownLatch.h
	CurrentThread.cc
	CurrentThread.h
	Mutex.h
	noncopyable.h
	Thread.cc
	Thread.h
)

add_library(muduo_base ${muduo_base_src})
target_include_directories(muduo_base
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(muduo_base
    PUBLIC
        ${muduo_base_libs}
)

target_sources(muduo_base
    PRIVATE
        ${muduo_base_src}
)
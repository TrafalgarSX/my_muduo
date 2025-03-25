cmake_minimum_required(VERSION 3.22)

project(muduo_base)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:include>
)
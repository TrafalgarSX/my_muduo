cmake_minimum_required(VERSION 3.22)

project(muduo_net)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

list(APPEND muduo_net_src
    EventLoop.cc
    EventLoop.h
)

add_library(muduo_net ${muduo_base_src})
target_include_directories(muduo_net
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		$<INSTALL_INTERFACE:include>
)
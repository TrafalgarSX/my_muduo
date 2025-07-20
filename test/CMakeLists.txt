cmake_minimum_required(VERSION 3.22)

project(muduo_test)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
list(APPEND muduo_test_libs
    GTest::gtest
    muduo_base
    muduo_net
)

list(APPEND muduo_test_src
    gtest_main.cc
)

# 修改此处，使用 muduo_net_src 而非 muduo_base_src
add_executable(muduo_test)
target_include_directories(muduo_test
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

# 使用变量展开
target_link_libraries(muduo_test
    PUBLIC
        ${muduo_test_libs}
)

target_sources(muduo_test
    PRIVATE
        ${muduo_test_src}
)
cmake_minimum_required(VERSION 3.22)

project(muduo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
list(APPEND INCLUDES
    ${PROJECT_SOURCE_DIR}/include
    ${VCPKG_INCLUDE_DIR}
)

list(APPEND COMPILE_OPTIONS 
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

list(APPEND SOURCES 
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)

list(APPEND LIBS
    fmt::fmt
    spdlog::spdlog
    date::date date::date-tz
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})
target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILE_OPTIONS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

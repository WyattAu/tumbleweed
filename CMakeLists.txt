cmake_minimum_required(VERSION 3.28)
project(tumbleweed CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tumbleweed tumbleweed.cpp)
target_compile_options(tumbleweed PRIVATE -O2 -Wall -Wextra -Wpedantic)
target_include_directories(tumbleweed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

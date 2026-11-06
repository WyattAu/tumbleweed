cmake_minimum_required(VERSION 3.28)
project(tumbleweed CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(tumbleweed tumbleweed.cpp)
target_compile_options(tumbleweed PRIVATE -O2 -Wall -Wextra -Wpedantic)
target_include_directories(tumbleweed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_executable(conformance tests/conformance.cpp)
target_compile_options(conformance PRIVATE -O2 -Wall -Wextra -Wpedantic)
target_include_directories(conformance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME conformance COMMAND conformance)

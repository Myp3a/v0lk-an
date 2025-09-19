cmake_minimum_required(VERSION 3.10.0)
project(pong VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}")

add_subdirectory(../fw fw)

add_executable(pong pong.cpp)

target_link_libraries(pong PUBLIC fw)

target_include_directories(pong PUBLIC "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}/../fw")

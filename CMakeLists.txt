cmake_minimum_required(VERSION 3.10)

project(concurrent-monotonic-buffer-allocator)

find_package(Threads REQUIRED)

add_executable(example example.cpp)
target_link_libraries(example PRIVATE Threads::Threads)
set_property(TARGET example PROPERTY CXX_STANDARD 20)

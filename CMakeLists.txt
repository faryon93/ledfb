cmake_minimum_required(VERSION 3.7)
project(ledfbd)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES ledfbd.c)
add_executable(ledfbd ${SOURCE_FILES})

target_link_libraries(ledfbd m)

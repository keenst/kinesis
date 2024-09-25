cmake_minimum_required(VERSION 3.13)
project(kinesis)
set(CMAKE_C_STANDARD 99)

set (SOURCE_FILES main.cpp)
add_executable(kinesis ${SOURCE_FILES})

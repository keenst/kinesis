cmake_minimum_required(VERSION 3.13)
project(kinesis C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set (SOURCE_FILES main.c)
add_executable(kinesis ${SOURCE_FILES})

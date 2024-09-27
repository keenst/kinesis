cmake_minimum_required(VERSION 3.13)
project(kinesis C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.c" "${SOURCE_DIR}/*.h")
add_executable(kinesis ${SOURCE_FILES})

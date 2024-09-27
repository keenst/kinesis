cmake_minimum_required(VERSION 3.13)
project(kinesis C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# glad
set(GLAD_PATH ${CMAKE_SOURCE_DIR}/libs/glad)

# Source files
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE SOURCE_FILES "${SOURCE_DIR}/*.c" "${SOURCE_DIR}/*.h")

add_executable(kinesis WIN32 ${SOURCE_FILES})
target_include_directories(kinesis PRIVATE ${GLAD_PATH})
target_link_libraries(kinesis PRIVATE OpenGL::GL)

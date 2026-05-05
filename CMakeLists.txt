cmake_minimum_required(VERSION 3.10)

project(custom_file_system C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(filefs
        src/main.c
        src/filesystem.c
)

target_include_directories(filefs PRIVATE include)
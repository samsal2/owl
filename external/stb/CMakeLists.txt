cmake_minimum_required(VERSION 3.17.3)

project(stb)

set(CMAKE_C_STANDARD 99)
set(C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(stb)

add_subdirectory(stb)

target_compile_options(stb PRIVATE -O3)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

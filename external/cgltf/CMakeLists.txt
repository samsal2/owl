cmake_minimum_required(VERSION 3.17.3)

project(cgltf)

set(CMAKE_C_STANDARD 99)
set(C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(cgltf)

add_subdirectory(cgltf)

target_compile_options(cgltf PRIVATE -O3)
target_include_directories(cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

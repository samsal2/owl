cmake_minimum_required(VERSION 3.17.3)

project(stb)

add_library(stb)

add_subdirectory(stb)

target_compile_options(stb PRIVATE -O3)
target_compile_features(stb PRIVATE c_std_99)
target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

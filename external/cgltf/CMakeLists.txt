cmake_minimum_required(VERSION 3.17.3)

project(cgltf)

add_library(cgltf)

add_subdirectory(cgltf)

target_compile_options(cgltf PRIVATE -O3)
target_compile_features(cgltf PRIVATE c_std_99)
target_include_directories(cgltf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

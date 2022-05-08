cmake_minimum_required(VERSION 3.23.1)

project(cgltf)

add_library(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        C_STANDARD          99 
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF)

target_include_directories(${PROJECT_NAME} 
  PUBLIC ${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  cgltf/cgltf.c
  cgltf/cgltf.h)

target_compile_options(${PROJECT_NAME} PRIVATE
	"$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:"
    -Wno-gcc-compat
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-O0
		-g
    -fsanitize=address
    -fsanitize=undefined
		-fsanitize=bounds
#   -fsanitize=leak
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Release>>:"
		-Ofast
  ">")

target_link_options(${PROJECT_NAME} PRIVATE
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
    -fsanitize=address
    -fsanitize=undefined
    -fsanitize=bounds
#   -fsanitize=leak
  ">")


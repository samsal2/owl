cmake_minimum_required(VERSION 3.23.1)

project(stb)

add_library(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        C_STANDARD          90 
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF)

target_include_directories(${PROJECT_NAME} 
  PUBLIC ${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  stb/stb_image.c
  stb/stb_image.h
  stb/stb_truetype.c
  stb/stb_truetype.h)

target_compile_options(${PROJECT_NAME} PRIVATE
	"$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:"
    -std=c99
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-O0
		-g
    -fsanitize=address
    -fsanitize=undefined
    -fsanitize=bounds
# 	-fsanitize=leak
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Release>>:"
		-Ofast
  ">")

target_link_options(${PROJECT_NAME} PRIVATE
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
    -fsanitize=address
    -fsanitize=undefined
    -fsanitize=bounds
#  	-fsanitize=leak
  ">")


cmake_minimum_required(VERSION 3.17.3)

project(owl)

option(OWL_BUILD_EXAMPLES "Build examples" ON)

# C options
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_EXTENSIONS OFF)
set(C_STANDARD_REQUIRED ON)

# Misc
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add library
add_library(${PROJECT_NAME})

# Find libraries
find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# add library sources
add_subdirectory(${PROJECT_NAME})

# import external libraries
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/stb)

if (OWL_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif (OWL_BUILD_EXAMPLES)

# link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC glfw) 
target_link_libraries(${PROJECT_NAME} PRIVATE stb_image)
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

# include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIR})

set(OWL_C_CLANG_BASE_COMPILE_FLAGS 
		-Wall
		-Wshadow
		-Wextra
		-Werror
		-Wpedantic
		-pedantic-errors
		-Walloca 
		-Wcast-qual 
		-Wconversion 
		-Wformat=2 
		-Wformat-security 
		-Wnull-dereference 
		-Wstack-protector 
		-Wstrict-overflow=3 
		-Wvla 
		-Warray-bounds 
		-Warray-bounds-pointer-arithmetic 
		-Wassign-enum 
		-Wbad-function-cast 
		-Wconditional-uninitialized 
		-Wconversion 
		-Wfloat-equal 
		-Wformat-type-confusion 
		-Widiomatic-parentheses 
		-Wimplicit-fallthrough 
		-Wloop-analysis 
		-Wpointer-arith 
		-Wshift-sign-overflow 
		-Wshorten-64-to-32 
		-Wswitch-enum 
		-Wtautological-constant-in-range-compare 
		-Wunreachable-code-aggressive

		-Wno-long-long       # C99 required extension
		-Wno-variadic-macros # C99 required extension
		-Wno-comment)        # C99 required extension

set(OWL_C_CLANG_DEBUG_COMPILE_FLAGS
		-O0
		-g
    -Wno-format-nonliteral # owl_dbg_log_
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds)

set(OWL_C_CLANG_RELEASE_COMPILE_FLAGS
		-O3)

set(OWL_C_CLANG_DEBUG_LINK_FLAGS
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds)
		
target_compile_options(${PROJECT_NAME} PRIVATE
	"$<$<C_COMPILER_ID:Clang,AppleClang>:${OWL_C_CLANG_BASE_COMPILE_FLAGS}>"
	"$<$<AND:$<C_COMPILER_ID:Clang,AppleClang>,$<CONFIG:Debug>>:${OWL_C_CLANG_DEBUG_COMPILE_FLAGS}>"
	"$<$<AND:$<C_COMPILER_ID:Clang,AppleClang>,$<CONFIG:Release>>:${OWL_C_CLANG_RELEASE_COMPILE_FLAGS}>")

target_link_options(${PROJECT_NAME} PUBLIC
	"$<$<AND:$<C_COMPILER_ID:Clang,AppleClang>,$<CONFIG:Debug>>:${OWL_C_CLANG_DEBUG_LINK_FLAGS}>")

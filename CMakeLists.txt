cmake_minimum_required(VERSION 3.17.3)

project(owl)

option(OWL_BUILD_EXAMPLES "Build examples" ON)

# Misc
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add library
add_library(owl)

# Find libraries
find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# add library sources
add_subdirectory(owl)

# import external libraries
add_subdirectory(${PROJECT_SOURCE_DIR}/external/glfw)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/stb)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/cgltf)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/freetype)

# add shaders
add_subdirectory(${PROJECT_SOURCE_DIR}/shaders)

if (OWL_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif (OWL_BUILD_EXAMPLES)

# link libraries
target_link_libraries(owl PRIVATE glfw) 
target_link_libraries(owl PRIVATE stb)
target_link_libraries(owl PRIVATE cgltf)
target_link_libraries(owl PRIVATE freetype)
target_link_libraries(owl PUBLIC ${Vulkan_LIBRARIES})

# include directories
target_include_directories(owl PUBLIC ${PROJECT_SOURCE_DIR})
target_include_directories(owl PUBLIC ${Vulkan_INCLUDE_DIR})

set(OWL_C_CLANG_BASE_COMPILE_FLAGS 
    -std=c90
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
    -Wframe-larger-than=20480

    -fdiagnostics-color=always

    -Wno-format-nonliteral # owl_dbg_log_
		-Wno-long-long       # C99 required extension
		-Wno-variadic-macros # C99 required extension
		-Wno-comment)        # C99 required extension

set(OWL_C_CLANG_DEBUG_COMPILE_FLAGS
		-O0
		-g
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
)

set(OWL_C_CLANG_RELEASE_COMPILE_FLAGS
		-O3)

set(OWL_C_CLANG_DEBUG_LINK_FLAGS
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds)
	
target_compile_features(owl PRIVATE c_std_99)

target_compile_options(owl PRIVATE
	"$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:"
    "${OWL_C_CLANG_BASE_COMPILE_FLAGS}>"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
    "${OWL_C_CLANG_DEBUG_COMPILE_FLAGS}>"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Release>>:"
    "${OWL_C_CLANG_RELEASE_COMPILE_FLAGS}>")

target_link_options(owl PUBLIC
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
    "${OWL_C_CLANG_DEBUG_LINK_FLAGS}>")


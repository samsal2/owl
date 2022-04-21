cmake_minimum_required(VERSION 3.17.3)

project(owl)

set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# declare the library 
add_library(owl)

# add sources
add_subdirectory(owl)

# find external libraries
find_package(Vulkan REQUIRED FATAL_ERROR)

# link libraries
target_link_libraries(owl 
  PUBLIC ${Vulkan_LIBRARIES}
  PUBLIC "$<$<BOOL:APPLE>:"
            ${PROJECT_SOURCE_DIR}/libs/glfw/macos/lib-x86_64/libglfw3.a
            "-framework Cocoa"
            "-framework IOKit"
          ">")

# includes
target_include_directories(owl 
  PUBLIC ${PROJECT_SOURCE_DIR}
  PUBLIC ${Vulkan_INCLUDE_DIR}
          "$<$<BOOL:APPLE>:"
            ${PROJECT_SOURCE_DIR}/libs/glfw/macos/include>
          ">")

# features
target_compile_features(owl PRIVATE c_std_99)

# compile options
target_compile_options(owl PRIVATE
	"$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:"
    -std=c99
		-Wall
		-Wshadow
		-Wextra
		-Werror
		-Wpedantic
		-pedantic-errors
		-Walloca 
		-Wvla 
    -fdiagnostics-color=always
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-O0
		-g
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Release>>:"
		-O3
  ">")

# compile definitions
target_compile_definitions(owl PRIVATE
  "$<$<CONFIG:DEBUG>:"
    OWL_ENABLE_VALIDATION
  ">")


# link options
target_link_options(owl PUBLIC
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
  ">")

# add examples
add_subdirectory(examples)


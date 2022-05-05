cmake_minimum_required(VERSION 3.23.1)

project(owl)

# declare the library 
add_library(${PROJECT_NAME})

# add sources
add_subdirectory(${PROJECT_NAME})

# find external libraries
find_package(Vulkan REQUIRED)

# link libraries
target_link_libraries(${PROJECT_NAME} 
  PUBLIC ${Vulkan_LIBRARIES}
  PUBLIC "$<$<BOOL:APPLE>:"
            ${PROJECT_SOURCE_DIR}/libs/glfw/macos/lib-x86_64/libglfw3.a
            "-framework Cocoa"
            "-framework IOKit"
          ">")

# includes
target_include_directories(${PROJECT_NAME} 
  PRIVATE ${CMAKE_BINARY_DIR}
  PUBLIC ${PROJECT_SOURCE_DIR}
  PUBLIC ${Vulkan_INCLUDE_DIR}
          "$<$<BOOL:APPLE>:"
            ${PROJECT_SOURCE_DIR}/libs/glfw/macos/include
          ">")

# features
set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        C_STANDARD          99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF)

# compile options
target_compile_options(${PROJECT_NAME} PRIVATE
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
		-Ofast
  ">")

# compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
  "$<$<CONFIG:DEBUG>:"
    OWL_ENABLE_VALIDATION
  ">")


# link options
target_link_options(${PROJECT_NAME} PUBLIC
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
  ">")

# add examples
add_subdirectory(examples)


cmake_minimum_required(VERSION 3.23.1)

project(owl)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# declare the library 
add_library(${PROJECT_NAME})

# features
set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        C_STANDARD          90 
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF)

# find external libraries
find_package(Vulkan REQUIRED)

add_subdirectory(deps/glfw)
add_subdirectory(deps/stb)
add_subdirectory(deps/cgltf)

# add sources
add_subdirectory(${PROJECT_NAME})
add_subdirectory(shaders)

# link libraries
target_link_libraries(${PROJECT_NAME} 
  PUBLIC ${Vulkan_LIBRARIES}
  PRIVATE cgltf
  PRIVATE glfw
  PRIVATE stb)

# includes
target_include_directories(${PROJECT_NAME} 
  PUBLIC ${Vulkan_INCLUDE_DIR}
  PUBLIC ${PROJECT_SOURCE_DIR})

# compile options
target_compile_options(${PROJECT_NAME} PUBLIC
	"$<$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>:"
		-Wall
		-Wshadow
		-Wextra
		-Werror
		-Wpedantic
		-pedantic-errors
		-Walloca 
		-Wvla 
    -fdiagnostics-color=always
    -Wno-long-long       # required C99 extension
    -Wno-variadic-macros # required C99 extension (for debug logging)
    -Wno-comment         # glslangValidator geenrates // comments
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
    -flto
  ">")

# link options
target_link_options(${PROJECT_NAME} PUBLIC
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Debug>>:"
    -fsanitize=address
    -fsanitize=undefined
    -fsanitize=bounds
#		-fsanitize=leak
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:C,Clang,AppleClang>,$<CONFIG:Release>>:"
    -flto
  ">")

# compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
  "$<$<CONFIG:DEBUG>:"
    OWL_ENABLE_VALIDATION
  ">")



# add examples
add_subdirectory(examples)


cmake_minimum_required(VERSION 3.23.1)

project(cimgui)

add_library(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
    PROPERTIES 
        CXX_STANDARD          11 
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF)

target_link_libraries(${PROJECT_NAME} PRIVATE
  PRIVATE glfw 
  PRIVATE ${Vulkan_LIBRARIES})

target_include_directories(${PROJECT_NAME} 
  PUBLIC ${PROJECT_NAME}
  PRIVATE ${PROJECT_NAME}/imgui
  PRIVATE ${Vulkan_INCLUDE_DIR})

target_sources(${PROJECT_NAME} PRIVATE
  cimgui/cimgui.cpp
  cimgui/cimgui.h
  cimgui/imgui/imgui.cpp
  cimgui/imgui/imgui_draw.cpp
  cimgui/imgui/imgui_widgets.cpp
  cimgui/imgui/imgui_tables.cpp
  cimgui/imgui/imgui_demo.cpp
  cimgui/imgui/backends/imgui_impl_vulkan.cpp
  cimgui/imgui/backends/imgui_impl_glfw.cpp)

target_compile_options(${PROJECT_NAME} PRIVATE
	"$<$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>:"
    -std=c++11
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-O0
		-g
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
  ">"
	"$<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>,$<CONFIG:Release>>:"
		-Ofast
  ">")

target_link_options(${PROJECT_NAME} PRIVATE
	"$<$<AND:$<COMPILE_LANG_AND_ID:CXX,Clang,AppleClang>,$<CONFIG:Debug>>:"
		-fsanitize=address
		-fsanitize=undefined
		-fsanitize=bounds
  ">")

target_compile_definitions(${PROJECT_NAME} PUBLIC
  PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1
  PUBLIC "$<$<BOOL:${WIN32}>:"
           IMGUI_IMPL_API=extern\t\"C\"\t__declspec\(dllexport\)
         ">"
  PUBLIC "$<$<NOT:$<BOOL:${WIN32}>>:"
           IMGUI_IMPL_API=extern\t\"C\"\t
         ">")


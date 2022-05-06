target_sources(${PROJECT_NAME} PRIVATE
  cgltf.c
  cgltf.h
  imconfig.h
  imgui.cpp
  imgui.h
  imgui_draw.cpp
  imgui_demo.cpp
  imgui_impl_glfw.cpp
  imgui_impl_glfw.h
  imgui_impl_vulkan.cpp
  imgui_impl_vulkan.h
  imgui_internal.h
  imgui_tables.cpp
  imgui_widgets.cpp
  imstb_rectpack.h
  imstb_truetype.h
  owl.h
  owl_camera.c
  owl_camera.h
  owl_client.c
  owl_client.h
  owl_draw_command.c
  owl_draw_command.h
  owl_font.c
  owl_font.h
  owl_internal.c
  owl_internal.h
  owl_imgui.cpp
  owl_imgui.h
  owl_model.c
  owl_model.h
  owl_renderer.c
  owl_renderer.h
  owl_types.h
  owl_vector_math.c
  owl_vector_math.h
  stb_image.c
  stb_image.h
  stb_truetype.c
  stb_truetype.h)

set(GLSL_SHADERS 
  owl_glsl_basic.vert
  owl_glsl_model.vert
  owl_glsl_pbr.vert
  owl_glsl_basic.frag
  owl_glsl_font.frag
  owl_glsl_model.frag
  owl_glsl_pbr.frag)

if ("${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" STREQUAL "")
  message(AUTHOR_WARNING "missing Vulkan_GLSLANG_VALIDATOR_EXECUTABLE, please compile shaders manually") 
else ()
  foreach(SHADER IN LISTS GLSL_SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    add_custom_command(OUTPUT ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${FILENAME}.spv.u32
      COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} ${SHADER} -V -x -o ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${FILENAME}.spv.u32
      DEPENDS ${SHADER}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
      list(APPEND SPV_SHADERS ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${FILENAME}.spv.u32)
  endForeach()

  add_custom_target(shaders DEPENDS ${SPV_SHADERS})
  add_dependencies(${PROJECT_NAME} shaders)
endif ()


message(AUTHOR_WARNING ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

target_include_directories(${PROJECT_NAME} 
  PRIVATE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})



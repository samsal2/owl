target_sources(${PROJECT_NAME} PRIVATE
  cgltf.c
  cgltf.h
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
  message(AUTHOR_WARNING "missing Vulkan_GLSLANG_VALIDATOR_EXECUTABLE"
                         "please compile shaders manually ${GLSL_SHADERS}") 
else ()
  foreach(SHADER IN LISTS GLSL_SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    set(OUT ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${FILENAME}.spv.u32)
    add_custom_command(OUTPUT ${OUT}
      COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} ${SHADER} -V -x -o ${OUT}
      DEPENDS ${SHADER}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
      list(APPEND SPV_SHADERS ${OUT})
  endForeach()

  add_custom_target(shaders DEPENDS ${SPV_SHADERS})
  add_dependencies(${PROJECT_NAME} shaders)
endif ()

target_include_directories(${PROJECT_NAME} 
  PRIVATE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})



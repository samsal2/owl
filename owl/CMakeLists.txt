target_sources(${PROJECT_NAME} PRIVATE
  owl.h
  owl_camera.c
  owl_camera.h
  owl_definitions.h
  owl_glsl_basic.frag
  owl_glsl_basic.vert
  owl_glsl_font.frag
  owl_glsl_model.frag
  owl_glsl_model.vert
  owl_glsl_pbr.frag
  owl_glsl_pbr.vert
  owl_glyph.h
  owl_internal.c
  owl_internal.h
  owl_io.c
  owl_io.h
  owl_model.c
  owl_model.h
  owl_quad.h
  owl_skybox.c
  owl_skybox.h
  owl_ui.c
  owl_ui.h
  owl_vector_math.c
  owl_vector_math.h
  owl_vk_attachment.c
  owl_vk_attachment.h
  owl_vk_context.c
  owl_vk_context.h
  owl_vk_font.c
  owl_vk_font.h
  owl_vk_frame.c
  owl_vk_frame.h
  owl_vk_frame_heap.c
  owl_vk_frame_heap.h
  owl_vk_frame_sync.c
  owl_vk_frame_sync.h
  owl_vk_garbage.c
  owl_vk_garbage.h
  owl_vk_im_command_buffer.c
  owl_vk_im_command_buffer.h
  owl_vk_image.c
  owl_vk_image.h
  owl_vk_pipeline_manager.c
  owl_vk_pipeline_manager.h
  owl_vk_renderer.c
  owl_vk_renderer.h
  owl_vk_stage_heap.c
  owl_vk_stage_heap.h
  owl_vk_swapchain.c
  owl_vk_swapchain.h
  owl_vk_types.h
  owl_window.c
  owl_window.h)

set(GLSL_SHADERS 
  owl_glsl_basic.vert
  owl_glsl_model.vert
  owl_glsl_pbr.vert
  owl_glsl_basic.frag
  owl_glsl_font.frag
  owl_glsl_model.frag
  owl_glsl_pbr.frag
  owl_glsl_skybox.vert
  owl_glsl_skybox.frag)

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



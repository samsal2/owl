target_sources(${PROJECT_NAME} PRIVATE
 owl_definitions.h
 owl_internal.c
 owl_internal.h
 owl_model.c
 owl_model.h
 owl_plataform.c
 owl_plataform.h
 owl_vector_math.c
 owl_vector_math.h
 owl_vk_draw.c
 owl_vk_draw.h
 owl_vk_font.c
 owl_vk_font.h
 owl_vk_frame.c
 owl_vk_frame.h
 owl_vk_init.c
 owl_vk_init.h
 owl_vk_misc.c
 owl_vk_misc.h
 owl_vk_pipeline.c
 owl_vk_pipeline.h
 owl_vk_renderer.c
 owl_vk_renderer.h
 owl_vk_skybox.c
 owl_vk_skybox.h
 owl_vk_swapchain.c
 owl_vk_swapchain.h
 owl_vk_texture.c
 owl_vk_texture.h
 owl_vk_types.h
 owl_vk_upload.c
 owl_vk_upload.h)



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



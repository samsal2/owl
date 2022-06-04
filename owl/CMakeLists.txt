target_sources(${PROJECT_NAME} PRIVATE
 owl-definitions.h
 owl-internal.c
 owl-internal.h
 owl-model.c
 owl-model.h
 owl-plataform.c
 owl-plataform.h
 owl-vector-math.c
 owl-vector-math.h
 owl-vk-draw.c
 owl-vk-draw.h
 owl-vk-font.c
 owl-vk-font.h
 owl-vk-frame.c
 owl-vk-frame.h
 owl-vk-init.c
 owl-vk-init.h
 owl-vk-misc.c
 owl-vk-misc.h
 owl-vk-pipeline.c
 owl-vk-pipeline.h
 owl-vk-renderer.c
 owl-vk-renderer.h
 owl-vk-skybox.c
 owl-vk-skybox.h
 owl-vk-swapchain.c
 owl-vk-swapchain.h
 owl-vk-texture.c
 owl-vk-texture.h
 owl-vk-types.h
 owl-vk-upload.c
 owl-vk-upload.h)



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



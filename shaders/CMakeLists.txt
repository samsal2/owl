find_program(GLSLANG_VALIDATOR NAME "glslangValidator")

if (NOT GLSLANG_VALIDATOR)
  message(WARNING "glslangValidator not found, recompiling shaders is disabled")
endif()

set(GLSL_SOURCE_FILES
  basic.frag
  basic.vert
  font.frag
  grid.frag
  grid.vert
  model.frag
  model.vert
  pbr.frag
  pbr.vert
  scene.frag
  scene.vert
  skin.frag
  skin.vert
  skybox.frag
  skybox.vert)

foreach (GLSL_FILE ${GLSL_SOURCE_FILES})
  get_filename_component(SHADER_EXT ${GLSL_FILE} EXT)
  get_filename_component(SHADER_NAME ${GLSL_FILE} NAME_WE)
  
  if (${SHADER_EXT} STREQUAL ".vert")
    set(SPIRV_FILE "${CMAKE_SOURCE_DIR}/${SHADER_NAME}_vertex.spv.u32")
  elseif (${SHADER_EXT} STREQUAL ".frag")
    set(SPIRV_FILE "${CMAKE_SOURCE_DIR}/${SHADER_NAME}_fragment.spv.u32")
  else (${SHADER_EXT} STREQUAL ".vert")
    message(WARNING "invalid shader extension in file ${GLSL_FILE}")
  endif (${SHADER_EXT} STREQUAL ".vert")

  add_custom_command(
    OUTPUT ${SPIRV_FILE}
    COMMAND ${GLSLANG_VALIDATOR} -V -x -o ${SPIRV_FILE} ${GLSL_FILE}
    DEPENDS ${GLSL})
 

  list(APPEND SPIRV_BINARY_FILES ${SPRIV_FILE}) 
endforeach(GLSL_FILE)


add_custom_target(owl_shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(owl owl_shaders)


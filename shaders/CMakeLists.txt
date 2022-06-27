set(GLSL_SHADERS 
  owl_basic.vert
  owl_basic.frag
  owl_font.frag
  owl_model.vert
  owl_model.frag
  owl_skybox.vert
  owl_skybox.frag
  owl_fluid_splat.comp
  owl_fluid_advect.comp
  owl_fluid_diverge.comp
  owl_fluid_curl.comp
  owl_fluid_vorticity.comp
  owl_fluid_pressure.comp
  owl_fluid_gradient_subtract.comp)

if ("${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}" STREQUAL "")
  message(AUTHOR_WARNING "missing Vulkan_GLSLANG_VALIDATOR_EXECUTABLE"
                         "please compile shaders manually ${GLSL_SHADERS}") 
else ()
  foreach(SHADER IN LISTS GLSL_SHADERS)
    get_filename_component(FILENAME ${SHADER} NAME)
    set(OUT ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}.spv.u32)
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
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})



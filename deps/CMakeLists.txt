add_subdirectory(glfw)
add_subdirectory(stb)
add_subdirectory(cgltf)

target_link_libraries(${PROJECT_NAME} PRIVATE
  glfw
  stb
  cgltf)

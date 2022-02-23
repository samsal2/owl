macro(add_example name)
  add_executable(${name} "${name}.c")
  target_link_libraries(${name} owl)
endmacro()

add_example(cloth_simulation)
add_example(model)


macro(add_example name)
  add_executable(${name} "${name}.c")
  target_link_libraries(${name} owl)
  target_compile_options(owl PRIVATE -Wall -Wextra -Werror -pedantic -pedantic -Ofast)
endmacro()

add_example(cloth_simulation)
add_example(model)

#include "owl_io.h"

#define GLFW_INCLUDE_NONDE
#include <GLFW/glfw3.h>

owl_public double
owl_io_time_stamp_get (void) {
  return glfwGetTime ();
}

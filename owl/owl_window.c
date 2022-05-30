#include "owl.h"

#include "owl_internal.h"

/* using GLFW until I get around building a plataform layer */

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
/* clang-format on */

#if 0
owl_private enum owl_window_mouse_button owl_as_mouse_key(owl_i32 type) {
  switch (type) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return OWL_WINDOW_MOUSE_BUTTON_LEFT;

  case GLFW_MOUSE_BUTTON_MIDDLE:
    return OWL_WINDOW_MOUSE_BUTTON_MIDDLE;

  case GLFW_MOUSE_BUTTON_RIGHT:
    return OWL_WINDOW_MOUSE_BUTTON_RIGHT;

  default:
    return 0;
  }
}

owl_private enum owl_window_button_state owl_as_mouse_state(owl_i32 state) {
  switch (state) {
  case GLFW_PRESS:
    return OWL_WINDOW_BUTTON_STATE_PRESS;

  case GLFW_RELEASE:
    return OWL_WINDOW_BUTTON_STATE_RELEASE;

  case GLFW_REPEAT:
    return OWL_WINDOW_BUTTON_STATE_REPEAT;

  default:
    return OWL_WINDOW_BUTTON_STATE_NONE;
  }
}

owl_private void owl_window_window_size_callback(GLFWwindow *data,
                                                  owl_i32 width,
                                                  owl_i32 height) {
  struct owl_window *w = glfwGetWindowUserPointer(data);

  w->window_width = width;
  w->window_height = height;
}

owl_private void owl_window_framebuffer_size_callback(GLFWwindow *data,
                                                       owl_i32 width,
                                                       owl_i32 height) {
  struct owl_window *w = glfwGetWindowUserPointer(data);

  w->framebuffer_width = width;
  w->framebuffer_height = height;
  w->framebuffer_ratio = (float)width / (float)height;
}

owl_private void owl_window_cursor_position_callback(GLFWwindow *data,
                                                      double x, double y) {
  struct owl_window *w = glfwGetWindowUserPointer(data);

  owl_v2_copy(w->cursor_position, w->previous_cursor_position);

  w->cursor_position[0] = 2.0F * ((float)x / (float)w->window_width) - 1.0F;

  w->cursor_position[1] = 2.0F * ((float)y / (float)w->window_height) - 1.0F;

  owl_v2_sub(w->cursor_position, w->previous_cursor_position,
             w->delta_cursor_position);
}

owl_private void owl_window_mouse_keys_callback(GLFWwindow *data,
                                                 owl_i32 button, owl_i32 action,
                                                 owl_i32 mod) {
  struct owl_window *w = glfwGetWindowUserPointer(data);
  enum owl_window_mouse_button key = owl_as_mouse_key(button);

  owl_unused(mod);

  w->mouse_buttons[key] = owl_as_mouse_state(action);
}

owl_private void owl_window_keyboard_keys_callback(GLFWwindow *data,
                                                    owl_i32 key, owl_i32 code,
                                                    owl_i32 action,
                                                    owl_i32 mods) {
  struct owl_window *w = glfwGetWindowUserPointer(data);

  owl_unused(code);
  owl_unused(action);
  owl_unused(mods);

  w->keyboard_keys[key] = owl_as_mouse_state(action);
}
#endif

enum owl_code
owl_window_init (struct owl_window *w, owl_i32 width, owl_i32 height,
                 char const *name) {
  enum owl_code code = OWL_SUCCESS;

  if (!glfwInit ()) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);

  w->opaque = glfwCreateWindow (width, height, name, NULL, NULL);

  if (!w->opaque) {
    code = OWL_ERROR_UNKNOWN;
    goto error_glfw_deinit;
  }
#if 0
  glfwSetWindowSizeCallback(w->data, owl_window_window_size_callback);
  glfwSetFramebufferSizeCallback(w->data, owl_window_framebuffer_size_callback);
  glfwSetCursorPosCallback(w->data, owl_window_cursor_position_callback);
  glfwSetMouseButtonCallback(w->data, owl_window_mouse_keys_callback);
  glfwSetKeyCallback(w->data, owl_window_keyboard_keys_callback);
#endif

  w->title = name;

  goto out;

error_glfw_deinit:
  glfwTerminate ();

out:
  return code;
}

owl_public void
owl_window_deinit (struct owl_window *w) {
  glfwDestroyWindow (w->opaque);
  glfwTerminate ();
}

#if defined(OWL_ENABLE_VALIDATION)

#define OWL_MAX_EXTENSIONS 64

owl_private char const *const *
owl_window_get_debug_instance_extensions (owl_u32 *count) {
  char const *const *extensions;
  owl_local_persist char const *names[OWL_MAX_EXTENSIONS];

  extensions = glfwGetRequiredInstanceExtensions (count);

  owl_assert (OWL_MAX_EXTENSIONS > *count);

  owl_memcpy (names, extensions, *count * sizeof (char const *));

  names[(*count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  return names;
}

#endif /* OWL_ENABLE_VALIDATION */

owl_public owl_i32
owl_window_is_done (struct owl_window *w) {
  return glfwWindowShouldClose (w->opaque);
}

owl_public void
owl_window_poll_events (struct owl_window *w) {
  owl_unused (w);

  glfwPollEvents ();
}

owl_public char const *const *
owl_window_get_instance_extensions (owl_u32 *count) {
#if defined(OWL_ENABLE_VALIDATION)
  return owl_window_get_debug_instance_extensions (count);
#else
  return glfwGetRequiredInstanceExtensions (count);
#endif
}

owl_public enum owl_code
owl_window_create_vk_surface (struct owl_window const *w, VkInstance instance,
                              VkSurfaceKHR *surface) {
  VkResult vk_result;

  vk_result = glfwCreateWindowSurface (instance, w->opaque, NULL, surface);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_public void
owl_window_get_framebuffer_size (struct owl_window const *w, owl_i32 *width,
                                 owl_i32 *height) {
  int iw;
  int ih;

  glfwGetFramebufferSize (w->opaque, &iw, &ih);

  *width = (owl_i32)iw;
  *height = (owl_i32)ih;
}

owl_public void
owl_window_get_window_size (struct owl_window const *w, owl_i32 *width,
                            owl_i32 *height) {
  int iw;
  int ih;

  glfwGetWindowSize (w->opaque, &iw, &ih);

  *width = (owl_i32)iw;
  *height = (owl_i32)ih;
}

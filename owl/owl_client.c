#include "owl_client.h"

#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
/* clang-format on */

OWL_INTERNAL enum owl_mouse_button owl_as_mouse_key(owl_i32 type) {
  switch (type) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return OWL_MOUSE_BUTTON_LEFT;

  case GLFW_MOUSE_BUTTON_MIDDLE:
    return OWL_MOUSE_BUTTON_MIDDLE;

  case GLFW_MOUSE_BUTTON_RIGHT:
    return OWL_MOUSE_BUTTON_RIGHT;

  default:
    return 0;
  }
}

OWL_INTERNAL enum owl_button_state owl_as_mouse_state(owl_i32 state) {
  switch (state) {
  case GLFW_PRESS:
    return OWL_BUTTON_STATE_PRESS;

  case GLFW_RELEASE:
    return OWL_BUTTON_STATE_RELEASE;

  case GLFW_REPEAT:
    return OWL_BUTTON_STATE_REPEAT;

  default:
    return OWL_BUTTON_STATE_NONE;
  }
}

OWL_INTERNAL void owl_client_window_size_callback(GLFWwindow *win, owl_i32 w,
                                                  owl_i32 h) {
  struct owl_client *client = glfwGetWindowUserPointer(win);

  client->window_width = w;
  client->window_height = h;
}

OWL_INTERNAL void owl_client_framebuffer_size_callback(GLFWwindow *win,
                                                       owl_i32 w, owl_i32 h) {
  struct owl_client *client = glfwGetWindowUserPointer(win);

  client->framebuffer_width = w;
  client->framebuffer_height = h;
  client->framebuffer_ratio = (float)w / (float)h;
}

OWL_INTERNAL void owl_client_cursor_position_callback(GLFWwindow *win, double x,
                                                      double y) {
  struct owl_client *client = glfwGetWindowUserPointer(win);

  OWL_V2_COPY(client->cursor_position, client->previous_cursor_position);

  client->cursor_position[0] =
      2.0F * ((float)x / (float)client->window_width) - 1.0F;

  client->cursor_position[1] =
      2.0F * ((float)y / (float)client->window_height) - 1.0F;

  OWL_V2_SUB(client->cursor_position, client->previous_cursor_position,
             client->delta_cursor_position);
}

OWL_INTERNAL void owl_client_mouse_keys_callback(GLFWwindow *win,
                                                 owl_i32 button, owl_i32 action,
                                                 owl_i32 mod) {
  struct owl_client *client = glfwGetWindowUserPointer(win);
  enum owl_mouse_button key = owl_as_mouse_key(button);

  OWL_UNUSED(mod);

  client->mouse_buttons[key] = owl_as_mouse_state(action);
}

OWL_INTERNAL void owl_client_keyboard_keys_callback(GLFWwindow *glfw,
                                                    owl_i32 key, owl_i32 code,
                                                    owl_i32 action,
                                                    owl_i32 mods) {
  struct owl_client *client = glfwGetWindowUserPointer(glfw);

  OWL_UNUSED(code);
  OWL_UNUSED(action);
  OWL_UNUSED(mods);

  client->keyboard_keys[key] = owl_as_mouse_state(action);
}

OWL_INTERNAL enum owl_code
owl_vk_create_surface_callback(struct owl_renderer const *r,
                               void const *user_data, VkSurfaceKHR *surface) {
  owl_i32 err;
  enum owl_code code = OWL_SUCCESS;
  struct owl_client const *client = user_data;

  err = glfwCreateWindowSurface(r->instance, client->window, NULL, surface);

  if (VK_SUCCESS != err) {
    code = OWL_ERROR_BAD_INIT;
  }

  return code;
}
enum owl_code owl_client_init(struct owl_client_init_desc const *desc,
                              struct owl_client *c) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  if (!glfwInit()) {
    code = OWL_ERROR_BAD_INIT;
    goto out;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  c->window =
      glfwCreateWindow(desc->width, desc->height, desc->title, NULL, NULL);

  glfwSetWindowUserPointer(c->window, c);
  glfwGetWindowSize(c->window, &c->window_width, &c->window_height);
  glfwGetFramebufferSize(c->window, &c->framebuffer_width,
                         &c->framebuffer_height);
  glfwSetWindowSizeCallback(c->window, owl_client_window_size_callback);
  glfwSetFramebufferSizeCallback(c->window,
                                 owl_client_framebuffer_size_callback);
  glfwSetCursorPosCallback(c->window, owl_client_cursor_position_callback);
  glfwSetMouseButtonCallback(c->window, owl_client_mouse_keys_callback);
  glfwSetKeyCallback(c->window, owl_client_keyboard_keys_callback);

  c->framebuffer_ratio =
      (float)c->framebuffer_width / (float)c->framebuffer_height;

  OWL_V2_ZERO(c->previous_cursor_position);
  OWL_V2_ZERO(c->cursor_position);

  for (i = 0; i < OWL_MOUSE_BUTTON_COUNT; ++i) {
    c->mouse_buttons[i] = OWL_BUTTON_STATE_NONE;
  }

  for (i = 0; i < OWL_KEYBOARD_KEY_LAST; ++i) {
    c->keyboard_keys[i] = OWL_BUTTON_STATE_NONE;
  }

  c->fps = 60.0F;
  c->delta_time_stamp = 0.16667;
  c->time_stamp = 0.0;
  c->previous_time_stamp = 0.0;
  c->title = desc->title;

out:
  return code;
}

void owl_client_deinit(struct owl_client *c) {
  glfwDestroyWindow(c->window);
  glfwTerminate();
}

#if defined(OWL_ENABLE_VALIDATION)

#define OWL_MAX_EXTENSIONS 64

OWL_INTERNAL char const *const *
owl_get_debug_instance_extensions(owl_u32 *count) {
  char const *const *extensions;
  OWL_LOCAL_PERSIST char const *names[OWL_MAX_EXTENSIONS];

  extensions = glfwGetRequiredInstanceExtensions(count);

  OWL_ASSERT(OWL_MAX_EXTENSIONS > *count);

  OWL_MEMCPY(names, extensions, *count * sizeof(char const *));

  names[(*count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  return names;
}

#endif /* OWL_ENABLE_VALIDATION */

enum owl_code
owl_client_fill_renderer_init_desc(struct owl_client const *client,
                                   struct owl_renderer_init_desc *desc) {
  owl_u32 count;
  enum owl_code code = OWL_SUCCESS;

  desc->window_width = client->window_width;
  desc->window_height = client->window_height;
  desc->framebuffer_ratio = client->framebuffer_ratio;
  desc->framebuffer_width = client->framebuffer_width;
  desc->framebuffer_height = client->framebuffer_height;
  desc->surface_user_data = client;
  desc->create_surface = owl_vk_create_surface_callback;

#if defined(OWL_ENABLE_VALIDATION)
  desc->instance_extensions = owl_get_debug_instance_extensions(&count);
#else  /* OWL_ENABLE_VALIDATION */
  desc->instance_extensions = glfwGetRequiredInstanceExtensions(&count);
#endif /* OWL_ENABLE_VALIDATION */

  desc->instance_extensions_count = (int)count;
  desc->name = client->title;

  return code;
}

owl_i32 owl_client_is_done(struct owl_client *c) {
  return glfwWindowShouldClose(c->window);
}

void owl_client_poll_events(struct owl_client *c) {
  glfwPollEvents();
  c->previous_time_stamp = c->time_stamp;
  c->time_stamp = glfwGetTime();
  c->delta_time_stamp = c->time_stamp - c->previous_time_stamp;
  c->fps = 1.0 / c->delta_time_stamp;
}

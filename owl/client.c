#include "client.h"

#include "internal.h"
#include "renderer.h"
#include "vector_math.h"

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
/* clang-format on */

OWL_INTERNAL enum owl_mouse_button owl_as_mouse_key_(owl_i32 type) {
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

OWL_INTERNAL enum owl_button_state owl_as_mouse_state_(owl_i32 state) {
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

OWL_INTERNAL void owl_window_size_callback_(GLFWwindow *window, owl_i32 width,
                                            owl_i32 height) {
  struct owl_client *c = glfwGetWindowUserPointer(window);

  c->window_width = width;
  c->window_height = height;
}

OWL_INTERNAL void owl_framebuffer_size_callback_(GLFWwindow *window,
                                                 owl_i32 width,
                                                 owl_i32 height) {
  struct owl_client *c = glfwGetWindowUserPointer(window);

  c->framebuffer_width = width;
  c->framebuffer_height = height;
}

OWL_INTERNAL void owl_cursor_position_callback_(GLFWwindow *window, double x,
                                                double y) {
  struct owl_client *c = glfwGetWindowUserPointer(window);

  OWL_V2_COPY(c->cursor_position, c->previous_cursor_position);

  c->cursor_position[0] = 2.0F * ((float)x / (float)c->window_width) - 1.0F;
  c->cursor_position[1] = 2.0F * ((float)y / (float)c->window_height) - 1.0F;

  OWL_V2_SUB(c->cursor_position, c->previous_cursor_position,
             c->d_cursor_position);
}

OWL_INTERNAL void owl_mouse_key_callback_(GLFWwindow *window, owl_i32 button,
                                          owl_i32 action, owl_i32 modifiers) {
  struct owl_client *c = glfwGetWindowUserPointer(window);
  enum owl_mouse_button key = owl_as_mouse_key_(button);

  OWL_UNUSED(modifiers);

  c->mouse_buttons[key] = owl_as_mouse_state_(action);
}

OWL_INTERNAL void owl_keyboard_key_callback_(GLFWwindow *glfw, owl_i32 key,
                                             owl_i32 scancode, owl_i32 action,
                                             owl_i32 mods) {
  struct owl_client *c = glfwGetWindowUserPointer(glfw);

  OWL_UNUSED(scancode);
  OWL_UNUSED(action);
  OWL_UNUSED(mods);

  c->keyboard_keys[key] = owl_as_mouse_state_(action);
}

OWL_INTERNAL enum owl_code
owl_vk_surface_init_callback_(struct owl_renderer const *renderer,
                              void const *data, VkSurfaceKHR *out) {
  enum owl_code code = OWL_SUCCESS;
  struct owl_client const *c = data;
  owl_i32 err =
      glfwCreateWindowSurface(renderer->instance, c->window, NULL, out);

  if (VK_SUCCESS != err)
    code = OWL_ERROR_BAD_INIT;

  return code;
}
enum owl_code owl_client_init(struct owl_client_init_desc const *desc,
                              struct owl_client *client) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  if (!glfwInit()) {
    code = OWL_ERROR_BAD_INIT;
    goto end;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  client->window =
      glfwCreateWindow(desc->width, desc->height, desc->title, NULL, NULL);

  glfwSetWindowUserPointer(client->window, client);
  glfwGetWindowSize(client->window, &client->window_width,
                    &client->window_height);
  glfwGetFramebufferSize(client->window, &client->framebuffer_width,
                         &client->framebuffer_height);
  glfwSetWindowSizeCallback(client->window, owl_window_size_callback_);
  glfwSetFramebufferSizeCallback(client->window,
                                 owl_framebuffer_size_callback_);
  glfwSetCursorPosCallback(client->window, owl_cursor_position_callback_);
  glfwSetMouseButtonCallback(client->window, owl_mouse_key_callback_);
  glfwSetKeyCallback(client->window, owl_keyboard_key_callback_);

  OWL_V2_ZERO(client->previous_cursor_position);
  OWL_V2_ZERO(client->cursor_position);

  for (i = 0; i < OWL_MOUSE_BUTTON_COUNT; ++i)
    client->mouse_buttons[i] = OWL_BUTTON_STATE_NONE;

  for (i = 0; i < OWL_KEYBOARD_KEY_LAST; ++i)
    client->keyboard_keys[i] = OWL_BUTTON_STATE_NONE;

  client->fps = 60.0F;
  client->d_time_stamp = 0.16667;
  client->time_stamp = 0.0;
  client->previous_time_stamp = 0.0;
  client->title = desc->title;

end:
  return code;
}

void owl_client_deinit(struct owl_client *client) {
  glfwDestroyWindow(client->window);
  glfwTerminate();
}

#if defined(OWL_ENABLE_VALIDATION)

#define OWL_MAX_EXTENSIONS 64

OWL_INTERNAL char const *const *
owl_get_debug_instance_extensions_(owl_u32 *count) {
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
  desc->framebuffer_width = client->framebuffer_width;
  desc->framebuffer_height = client->framebuffer_height;
  desc->surface_user_data = client;
  desc->create_surface = owl_vk_surface_init_callback_;

#if defined(OWL_ENABLE_VALIDATION)
  desc->instance_extensions = owl_get_debug_instance_extensions_(&count);
#else  /* OWL_ENABLE_VALIDATION */
  rid->instance_extensions = glfwGetRequiredInstanceExtensions(&count);
#endif /* OWL_ENABLE_VALIDATION */

  desc->instance_extensions_count = (int)count;
  desc->name = client->title;

  return code;
}

owl_i32 owl_client_is_done(struct owl_client *client) {
  return glfwWindowShouldClose(client->window);
}

void owl_client_poll_events(struct owl_client *client) {
  glfwPollEvents();
  client->previous_time_stamp = client->time_stamp;
  client->time_stamp = glfwGetTime();
  client->d_time_stamp = client->time_stamp - client->previous_time_stamp;
  client->fps = 1.0F / client->d_time_stamp;
}

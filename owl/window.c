#include <owl/internal.h>
#include <owl/math.h>
#include <owl/types.h>
#include <owl/vk_config.h>
#include <owl/vk_renderer.h>
#include <owl/window.h>
/* clang-format off */
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

OWL_INTERNAL enum owl_mouse_btn owl_glfw_to_mouse_btn_(int type) {
#define OWL_MOUSE_BUTTON_CASE(type)                                          \
  case GLFW_MOUSE_BUTTON_##type:                                             \
    return OWL_MOUSE_BUTTON_##type

  switch (type) {
    OWL_MOUSE_BUTTON_CASE(LEFT);
    OWL_MOUSE_BUTTON_CASE(MIDDLE);
    OWL_MOUSE_BUTTON_CASE(RIGHT);

  default:
    return 0;
  }

#undef OWL_MOUSE_BUTTON_CASE
}

OWL_INTERNAL enum owl_btn_state owl_glfw_to_mouse_state_(int state) {
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

OWL_INTERNAL void owl_glfw_window_cb_(GLFWwindow *window, int width,
                                      int height) {
  struct owl_window *w = glfwGetWindowUserPointer(window);

  w->window_width = width;
  w->window_height = height;
}

OWL_INTERNAL void owl_glfw_framebuffer_cb_(GLFWwindow *handle, int width,
                                           int height) {
  struct owl_window *w = glfwGetWindowUserPointer(handle);

  w->framebuffer_width = width;
  w->framebuffer_height = height;
}

OWL_INTERNAL void owl_glfw_cursor_pos_cb_(GLFWwindow *handle, double x,
                                          double y) {
  struct owl_window *w = glfwGetWindowUserPointer(handle);

  w->cursor.cur_pos[0] = 2.0F * ((float)x / (float)w->window_width) - 1.0F;
  w->cursor.cur_pos[1] = 2.0F * ((float)y / (float)w->window_height) - 1.0F;
}

OWL_INTERNAL void owl_glfw_mouse_cb_(GLFWwindow *window, int button,
                                     int action, int modifiers) {
  struct owl_window *w = glfwGetWindowUserPointer(window);
  enum owl_mouse_btn b = owl_glfw_to_mouse_btn_(button);

  OWL_UNUSED(modifiers);

  w->mouse[b] = owl_glfw_to_mouse_state_(action);
}

OWL_INTERNAL void owl_glfw_keyboard_cb_(GLFWwindow *window, int key,
                                        int scancode, int action, int mods) {
  struct owl_window *w = glfwGetWindowUserPointer(window);

  OWL_UNUSED(scancode);
  OWL_UNUSED(action);
  OWL_UNUSED(mods);

  w->keyboard[key] = owl_glfw_to_mouse_state_(action);
}

OWL_INTERNAL enum owl_code
owl_init_vk_surface_cb_(void const *data,
                        struct owl_vk_renderer const *renderer,
                        VkSurfaceKHR *out) {
  VkSurfaceKHR *surface = out;
  struct owl_window const *window = data;
  int err = glfwCreateWindowSurface(renderer->instance, window->handle, NULL,
                                    surface);
  if (VK_SUCCESS != err)
    return OWL_ERROR_BAD_INIT;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_init_timer_(struct owl_window *window) {
  window->timer_now = 0.0;
  window->timer_past = 0.0;
  window->timer_dt = 0.16667;
}

OWL_INTERNAL void owl_update_timer_(struct owl_window *window) {
  window->timer_past = window->timer_now;
  window->timer_now = glfwGetTime();
  window->timer_dt = window->timer_now - window->timer_past;
}

OWL_INTERNAL void owl_update_prev_cursor_(struct owl_window *window) {
  OWL_COPY_V2(window->cursor.cur_pos, window->cursor.prev_pos);
}

OWL_INTERNAL enum owl_code owl_init_window(int width, int height,
                                           char const *title,
                                           struct owl_window *window) {
  if (GLFW_FALSE == glfwInit())
    return OWL_ERROR_BAD_INIT;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window->handle = glfwCreateWindow(width, height, title, NULL, NULL);

  glfwSetWindowUserPointer(window->handle, window);
  glfwGetWindowSize(window->handle, &window->window_width,
                    &window->window_height);
  glfwGetFramebufferSize(window->handle, &window->framebuffer_width,
                         &window->framebuffer_height);
  glfwSetWindowSizeCallback(window->handle, owl_glfw_window_cb_);
  glfwSetFramebufferSizeCallback(window->handle, owl_glfw_framebuffer_cb_);
  glfwSetCursorPosCallback(window->handle, owl_glfw_cursor_pos_cb_);
  glfwSetMouseButtonCallback(window->handle, owl_glfw_mouse_cb_);
  glfwSetKeyCallback(window->handle, owl_glfw_keyboard_cb_);

  OWL_ZERO_V2(window->cursor.prev_pos);
  OWL_ZERO_V2(window->cursor.cur_pos);

  {
    int i;
    for (i = 0; i < OWL_MOUSE_BUTTON_COUNT; ++i)
      window->mouse[i] = OWL_BUTTON_STATE_NONE;

    for (i = 0; i < OWL_KEYBOARD_KEY_LAST; ++i)
      window->keyboard[i] = OWL_BUTTON_STATE_NONE;
  }

  owl_init_timer_(window);

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_window(struct owl_window *window) {
  glfwDestroyWindow(window->handle);
  glfwTerminate();
}

#ifndef NDEBUG
#define OWL_MAX_EXTENSIONS 64
enum owl_code owl_fill_vk_config(struct owl_window const *window,
                                 struct owl_vk_config *config) {
  OwlU32 count;
  OWL_LOCAL_PERSIST char const *names[OWL_MAX_EXTENSIONS];

  config->framebuffer_width = (OwlU32)window->framebuffer_width;
  config->framebuffer_height = (OwlU32)window->framebuffer_height;

  config->surface_user_data = window;
  config->create_surface = owl_init_vk_surface_cb_;
  config->instance_extensions = glfwGetRequiredInstanceExtensions(&count);

  OWL_ASSERT(OWL_MAX_EXTENSIONS > count);

  OWL_MEMCPY(names, config->instance_extensions,
             count * sizeof(char const *));
  names[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  config->instance_extensions = names;
  config->instance_extension_count = count;

  return OWL_SUCCESS;
}
#undef OWL_MAX_EXTENSIONS
#else
enum owl_code owl_fill_vk_config(struct owl_window const *window,
                                 struct owl_vk_config *config) {
  OwlU32 count;

  config->width = (OwlU32)window->framebuffer_width;
  config->height = (OwlU32)window->framebuffer_height;

  config->user_data = window;
  config->create_surface = owl_init_vk_surface_cb_;

  config->instance_extensions = glfwGetRequiredInstanceExtensions(&count);
  config->instance_extension_count = count;

  return OWL_SUCCESS;
}
#endif

enum owl_code owl_create_window(int width, int height, char const *title,
                                struct owl_window **window) {
  *window = OWL_MALLOC(sizeof(**window));
  return owl_init_window(width, height, title, *window);
}

void owl_destroy_window(struct owl_window *window) {
  owl_deinit_window(window);
  OWL_FREE(window);
}

int owl_should_window_close(struct owl_window const *window) {
  return glfwWindowShouldClose(window->handle);
}

void owl_poll_events(struct owl_window *window) {
  owl_update_prev_cursor_(window);
  glfwPollEvents();
  owl_update_timer_(window);
}

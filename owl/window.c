#include <owl/internal.h>
#include <owl/math.h>
#include <owl/renderer_internal.h>
#include <owl/types.h>
#include <owl/window.h>
#include <owl/window_internal.h>
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

  w->size.width = width;
  w->size.height = height;
}

OWL_INTERNAL void owl_glfw_framebuffer_cb_(GLFWwindow *handle, int width,
                                           int height) {
  struct owl_window *w = glfwGetWindowUserPointer(handle);

  w->framebuffer.width = width;
  w->framebuffer.height = height;
}

OWL_INTERNAL void owl_glfw_cursor_pos_cb_(GLFWwindow *handle, double x,
                                          double y) {
  struct owl_window *w = glfwGetWindowUserPointer(handle);

  w->cursor.current[0] = 2.0F * ((float)x / (float)w->size.width) - 1.0F;
  w->cursor.current[1] = 2.0F * ((float)y / (float)w->size.height) - 1.0F;
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
owl_vk_init_surface_(struct owl_renderer const *renderer, void const *data,
                     void *out) {
  VkSurfaceKHR *surface = out;
  struct owl_window const *window = data;

  if (VK_SUCCESS != glfwCreateWindowSurface(renderer->instance,
                                            window->handle, NULL, surface))
    return OWL_ERROR_BAD_INIT;

  return OWL_SUCCESS;
}

enum owl_code owl_init_window(int width, int height, char const *title,
                              struct owl_window *window) {
  if (GLFW_FALSE == glfwInit())
    return OWL_ERROR_BAD_INIT;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window->handle = glfwCreateWindow(width, height, title, NULL, NULL);

  glfwSetWindowUserPointer(window->handle, window);
  glfwGetWindowSize(window->handle, &window->size.width,
                    &window->size.height);
  glfwGetFramebufferSize(window->handle, &window->framebuffer.width,
                         &window->framebuffer.height);
  glfwSetWindowSizeCallback(window->handle, owl_glfw_window_cb_);
  glfwSetFramebufferSizeCallback(window->handle, owl_glfw_framebuffer_cb_);
  glfwSetCursorPosCallback(window->handle, owl_glfw_cursor_pos_cb_);
  glfwSetMouseButtonCallback(window->handle, owl_glfw_mouse_cb_);
  glfwSetKeyCallback(window->handle, owl_glfw_keyboard_cb_);

  OWL_ZERO_V2(window->cursor.previous);
  OWL_ZERO_V2(window->cursor.current);

  {
    int i;
    for (i = 0; i < OWL_MOUSE_BUTTON_COUNT; ++i)
      window->mouse[i] = OWL_BUTTON_STATE_NONE;

    for (i = 0; i < OWL_KEYBOARD_KEY_LAST; ++i)
      window->keyboard[i] = OWL_BUTTON_STATE_NONE;
  }

  return OWL_SUCCESS;
}

void owl_deinit_window(struct owl_window *window) {
  glfwDestroyWindow(window->handle);
  glfwTerminate();
}

#ifndef OWL_ENABLE_VALIDATION
enum owl_code owl_vk_fill_info(struct owl_window const *window,
                               struct owl_vk_plataform *plataform) {
  OwlU32 count;

  plataform->surface_data = window;
  plataform->get_surface = owl_vk_init_surface_;

  extensions->extensions = glfwGetRequiredInstanceExtensions(&count);
  extensions->extension_count = count;

  return OWL_SUCCESS;
}
#else
#define OWL_MAX_EXTENSIONS 64
enum owl_code owl_vk_fill_info(struct owl_window const *window,
                               struct owl_vk_plataform *plataform) {
  OwlU32 count;
  OWL_LOCAL_PERSIST char const *names[OWL_MAX_EXTENSIONS];

  plataform->surface_data = window;
  plataform->get_surface = owl_vk_init_surface_;
  plataform->extensions = glfwGetRequiredInstanceExtensions(&count);

  OWL_ASSERT(OWL_MAX_EXTENSIONS > count);

  OWL_MEMCPY(names, plataform->extensions, count * sizeof(char const *));
  names[count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  plataform->extensions = names;
  plataform->extension_count = count;

  return OWL_SUCCESS;
}
#undef OWL_MAX_EXTENSIONS
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
  OWL_UNUSED(window);
  OWL_COPY_V2(window->cursor.current, window->cursor.previous);
  glfwPollEvents();
}

void owl_start_timer(struct owl_timer *timer) {
  timer->start = glfwGetTime();
}

OwlSeconds owl_end_timer(struct owl_timer *timer) {
  timer->end = glfwGetTime();
  return timer->end - timer->start;
}

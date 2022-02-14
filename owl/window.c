#include "window.h"

#include "internal.h"
#include "renderer.h"
#include "vmath.h"

/* clang-format off */
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

OWL_INTERNAL enum owl_mouse_button owl_as_mouse_button_(int type) {
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

OWL_INTERNAL enum owl_button_state owl_as_mouse_state_(int state) {
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

OWL_INTERNAL void owl_window_callback_(GLFWwindow *glfw, int width,
                                       int height) {
  struct owl_window *w = glfwGetWindowUserPointer(glfw);

  w->window_width = width;
  w->window_height = height;
}

OWL_INTERNAL void owl_framebuffer_callback_(GLFWwindow *glfw, int width,
                                            int height) {
  struct owl_window *w = glfwGetWindowUserPointer(glfw);

  w->framebuffer_width = width;
  w->framebuffer_height = height;
}

OWL_INTERNAL void owl_cursor_position_callback_(GLFWwindow *glfw, double x,
                                                double y) {
  struct owl_window *w = glfwGetWindowUserPointer(glfw);

  w->input.cursor_position[0] =
      2.0F * ((float)x / (float)w->window_width) - 1.0F;

  w->input.cursor_position[1] =
      2.0F * ((float)y / (float)w->window_height) - 1.0F;
}

OWL_INTERNAL void owl_mouse_callback_(GLFWwindow *glfw, int button, int action,
                                      int modifiers) {
  struct owl_window *w = glfwGetWindowUserPointer(glfw);
  enum owl_mouse_button b = owl_as_mouse_button_(button);

  OWL_UNUSED(modifiers);

  w->input.mouse[b] = owl_as_mouse_state_(action);
}

OWL_INTERNAL void owl_keyboard_callback_(GLFWwindow *glfw, int key,
                                         int scancode, int action, int mods) {
  struct owl_window *w = glfwGetWindowUserPointer(glfw);

  OWL_UNUSED(scancode);
  OWL_UNUSED(action);
  OWL_UNUSED(mods);

  w->input.keyboard[key] = owl_as_mouse_state_(action);
}

OWL_INTERNAL enum owl_code
owl_vk_surface_init_callback_(struct owl_vk_renderer const *r, void const *data,
                              VkSurfaceKHR *out) {
  struct owl_window const *w = data;
  int err = glfwCreateWindowSurface(r->instance, w->data, NULL, out);

  if (VK_SUCCESS != err)
    return OWL_ERROR_BAD_INIT;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_window_init_timer_(struct owl_window *w) {
  w->input.time = 0.0;
  w->input.past_time = 0.0;
  w->input.dt_time = 0.16667;
}

OWL_INTERNAL void owl_window_update_timer_(struct owl_window *w) {
  w->input.past_time = w->input.time;
  w->input.time = glfwGetTime();
  w->input.dt_time = w->input.time - w->input.past_time;
}

OWL_INTERNAL void owl_window_update_prev_cursor_(struct owl_window *w) {
  OWL_COPY_V2(w->input.cursor_position, w->input.prev_cursor_position);
}

OWL_GLOBAL int global_window_count = 0;

OWL_INTERNAL enum owl_code owl_window_ensure_glfw_(void) {
  if (!global_window_count)
    if (GLFW_FALSE == glfwInit())
      return OWL_ERROR_BAD_INIT;

  return OWL_SUCCESS;
}

OWL_INTERNAL void *
owl_window_create_handle_(struct owl_window_desc const *desc) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  ++global_window_count;
  return glfwCreateWindow(desc->width, desc->height, desc->title, NULL, NULL);
}

OWL_INTERNAL void owl_window_set_callbacks_(struct owl_window *w) {
  /* FIXME: not exactly setting up callbacks */
  glfwSetWindowUserPointer(w->data, w);
  glfwGetWindowSize(w->data, &w->window_width, &w->window_height);
  glfwGetFramebufferSize(w->data, &w->framebuffer_width,
                         &w->framebuffer_height);

  glfwSetWindowSizeCallback(w->data, owl_window_callback_);
  glfwSetFramebufferSizeCallback(w->data, owl_framebuffer_callback_);
  glfwSetCursorPosCallback(w->data, owl_cursor_position_callback_);
  glfwSetMouseButtonCallback(w->data, owl_mouse_callback_);
  glfwSetKeyCallback(w->data, owl_keyboard_callback_);
}

OWL_INTERNAL void owl_window_init_input_(struct owl_window *w) {
  int i;

  OWL_ZERO_V2(w->input.prev_cursor_position);
  OWL_ZERO_V2(w->input.cursor_position);

  for (i = 0; i < OWL_MOUSE_BUTTON_COUNT; ++i)
    w->input.mouse[i] = OWL_BUTTON_STATE_NONE;

  for (i = 0; i < OWL_KEYBOARD_KEY_LAST; ++i)
    w->input.keyboard[i] = OWL_BUTTON_STATE_NONE;
}

enum owl_code owl_window_init(struct owl_window_desc const *desc,
                              struct owl_input_state **input,
                              struct owl_window *w) {
  owl_window_ensure_glfw_();

  if (!input)
    return OWL_ERROR_BAD_PTR;

  w->data = owl_window_create_handle_(desc);

  owl_window_set_callbacks_(w);
  owl_window_init_input_(w);
  owl_window_init_timer_(w);

  *input = &w->input;
  w->title = desc->title;

  return OWL_SUCCESS;
}

void owl_window_deinit(struct owl_window *w) {
  glfwDestroyWindow(w->data);

  if (!--global_window_count)
    glfwTerminate();
}

#ifndef NDEBUG
#define OWL_MAX_EXTENSIONS 64

OWL_INTERNAL char const *const *
owl_get_dbg_instance_extensions_(owl_u32 *count) {
  char const *const *extensions;
  OWL_LOCAL_PERSIST char const *names[OWL_MAX_EXTENSIONS];

  extensions = glfwGetRequiredInstanceExtensions(count);

  OWL_ASSERT(OWL_MAX_EXTENSIONS > *count);

  OWL_MEMCPY(names, extensions, *count * sizeof(char const *));

  names[(*count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  return names;
}

#undef OWL_MAX_EXTENSIONS
#endif

enum owl_code
owl_window_fill_vk_renderer_desc(struct owl_window const *w,
                                 struct owl_vk_renderer_desc *desc) {
  owl_u32 count;

  desc->framebuffer_width = w->framebuffer_width;
  desc->framebuffer_height = w->framebuffer_height;
  desc->surface_user_data = w;
  desc->create_surface = owl_vk_surface_init_callback_;

#ifndef NDEBUG
  desc->instance_extensions = owl_get_dbg_instance_extensions_(&count);
#else
  desc->instance_extensions = glfwGetRequiredInstanceExtensions(count);
#endif

  desc->instance_extension_count = (int)count;
  desc->name = w->title;

  return OWL_SUCCESS;
}

int owl_window_is_done(struct owl_window *w) {
  return glfwWindowShouldClose(w->data);
}

void owl_window_poll(struct owl_window *w) {
  owl_window_update_prev_cursor_(w);
  glfwPollEvents();
  owl_window_update_timer_(w);
}

enum owl_code owl_window_create(struct owl_window_desc const *desc,
                                struct owl_input_state **input,
                                struct owl_window **w) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*w = OWL_MALLOC(sizeof(**w))))
    return OWL_ERROR_BAD_ALLOC;

  if (OWL_SUCCESS != (code = owl_window_init(desc, input, *w)))
    OWL_FREE(*w);

  return code;
}

void owl_window_destroy(struct owl_window *w) {
  owl_window_deinit(w);
  OWL_FREE(w);
}

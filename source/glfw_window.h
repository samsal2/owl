#ifndef OWL_SOURCE_GLFW_WINDOW_H_
#define OWL_SOURCE_GLFW_WINDOW_H_

#include "../include/owl/code.h"
#include "../include/owl/input.h"

/* clang-format off */
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

struct owl_glfw_window {
  GLFWwindow *handle;

  int window_width;
  int window_height;

  int framebuffer_width;
  int framebuffer_height;

  struct owl_input_state input;
};

enum owl_code owl_init_glfw_window(int width, int height, char const *title,
                                   struct owl_input_state **input,
                                   struct owl_glfw_window *window);

void owl_deinit_glfw_window(struct owl_glfw_window *window);

int owl_is_glfw_window_done(struct owl_glfw_window *window);

void owl_poll_glfw_events(struct owl_glfw_window *window);

enum owl_code owl_glfw_fill_vk_config(struct owl_glfw_window const *window,
                                      struct owl_vk_config *config);

#endif

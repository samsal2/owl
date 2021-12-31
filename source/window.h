#ifndef OWL_SOURCE_WINDOW_H_
#define OWL_SOURCE_WINDOW_H_

#include "glfw_window.h"

struct owl_window {
  struct owl_glfw_window glfw;
};

enum owl_code owl_fill_vk_config(struct owl_window const *window,
                                 struct owl_vk_config *config);

#endif

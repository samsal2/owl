#ifndef OWL_WINDOW_H_
#define OWL_WINDOW_H_

#include <owl/owl.h>

struct owl_vk_plataform;

struct owl_window {
  void *handle;

  int window_width;
  int window_height;
  int framebuffer_width;
  int framebuffer_height;

  struct owl_input_state input;
};

enum owl_code owl_init_window(struct owl_window_desc const *desc,
                              struct owl_input_state **input,
                              struct owl_window *window);

void owl_deinit_window(struct owl_window *window);

enum owl_code owl_fill_vk_plataform(struct owl_window const *window,
                                    struct owl_vk_plataform *plataform);

#endif

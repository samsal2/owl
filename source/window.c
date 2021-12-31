#include "window.h"

#include "glfw_window.h"
#include "internal.h"

OWL_INTERNAL enum owl_code owl_init_window(int width, int height,
                                           char const *title,
                                           struct owl_input_state **input,
                                           struct owl_window *window) {
  return owl_init_glfw_window(width, height, title, input, &window->glfw);
}

OWL_INTERNAL void owl_deinit_window(struct owl_window *window) {
  owl_deinit_glfw_window(&window->glfw);
}

enum owl_code owl_create_window(int width, int height, char const *title,
                                struct owl_input_state **input,
                                struct owl_window **window) {
  enum owl_code err = OWL_SUCCESS;

  if (!(*window = OWL_MALLOC(sizeof(**window))))
    return OWL_ERROR_BAD_ALLOC;

  err = owl_init_window(width, height, title, input, *window);

  if (OWL_SUCCESS != err)
    OWL_FREE(*window);

  return err;
}

void owl_destroy_window(struct owl_window *window) {
  owl_deinit_window(window);
  OWL_FREE(window);
}

void owl_poll_window_input(struct owl_window *window) {
  owl_poll_glfw_events(&window->glfw);
}

int owl_is_window_done(struct owl_window *window) {
  return owl_is_glfw_window_done(&window->glfw);
}

enum owl_code owl_fill_vk_config(struct owl_window const *window,
                                 struct owl_vk_config *config) {
  return owl_glfw_fill_vk_config(&window->glfw, config);
}

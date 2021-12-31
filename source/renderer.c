#include "renderer.h"

#include "../include/owl/renderer.h"
#include "internal.h"
#include "vk_config.h"
#include "vk_renderer.h"
#include "window.h"

enum owl_code owl_create_renderer(struct owl_window *window,
                                  struct owl_vk_renderer **renderer) {
  struct owl_vk_config config;
  enum owl_code err = OWL_SUCCESS;

  if (!(*renderer = OWL_MALLOC(sizeof(**renderer))))
    return OWL_ERROR_BAD_ALLOC;

  owl_fill_vk_config(window, &config);

  if (OWL_SUCCESS != (err = owl_init_vk_renderer(&config, *renderer)))
    OWL_FREE(*renderer);

  return err;
}

enum owl_code owl_recreate_swapchain(struct owl_window *window,
                                     struct owl_vk_renderer *renderer) {
  struct owl_vk_config config;
  owl_fill_vk_config(window, &config);
  return owl_reinit_vk_swapchain(&config, renderer);
}

void owl_destroy_renderer(struct owl_vk_renderer *renderer) {
  owl_deinit_vk_renderer(renderer);
  OWL_FREE(renderer);
}

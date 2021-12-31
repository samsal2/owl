#ifndef OWL_INCLUDE_RENDERER_H_
#define OWL_INCLUDE_RENDERER_H_

#include "code.h"

struct owl_window;
struct owl_vk_renderer;

enum owl_code owl_create_renderer(struct owl_window *window,
                                  struct owl_vk_renderer **renderer);

enum owl_code owl_recreate_swapchain(struct owl_window *window,
                                     struct owl_vk_renderer *renderer);

void owl_destroy_renderer(struct owl_vk_renderer *renderer);

#endif

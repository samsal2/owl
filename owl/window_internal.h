#ifndef OWL_WINDOW_INL_
#define OWL_WINDOW_INL_

#include <owl/code.h>
#include <owl/fwd.h>
#include <owl/types.h>

struct owl_vk_plataform;

enum owl_code owl_init_window(int width, int height, char const *title,
                              struct owl_window *window);

void owl_deinit_window(struct owl_window *window);

enum owl_code owl_vk_fill_info(struct owl_window const *window,
                               struct owl_vk_plataform *plataform);

#endif

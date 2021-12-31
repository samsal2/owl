#ifndef OWL_INCLUDE_FONT_H_
#define OWL_INCLUDE_FONT_H_

#include "code.h"

struct owl_font;
struct owl_vk_renderer;

enum owl_code owl_create_font(struct owl_vk_renderer *renderer, int size,
                              char const *path, struct owl_font **font);

void owl_destroy_font(struct owl_vk_renderer *renderer, struct owl_font *font);

#endif

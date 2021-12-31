#ifndef OWL_INCLUDE_TEXT_H_
#define OWL_INCLUDE_TEXT_H_

#include "../include/owl/code.h"
#include "../include/owl/types.h"

struct owl_font;
struct owl_vk_renderer;

enum owl_code owl_submit_text(struct owl_vk_renderer *renderer,
                              struct owl_font const *font, OwlV2 const pos,
                              OwlV3 const color, char const *text);

#endif

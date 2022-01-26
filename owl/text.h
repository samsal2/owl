#ifndef OWL_TEXT_H_
#define OWL_TEXT_H_

#include "types.h"

struct owl_vk_renderer;

struct owl_text_cmd {
  char const *text;
  struct owl_font const *font;
  owl_v2 pos;
  owl_v3 color;
};

enum owl_code owl_text_cmd_submit(struct owl_vk_renderer *renderer,
                                  struct owl_text_cmd const *cmd);

#endif

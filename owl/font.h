#ifndef OWL_FONT_H_
#define OWL_FONT_H_

#include "image.h"

#define OWL_FONT_MAX_GLYPHS 128

struct owl_glyph {
  int offset;
  owl_v2i advance;
  owl_v2i size;
  owl_v2i bearing;
};

struct owl_font {
  int size;
  int atlas_width;
  int atlas_height;
  struct owl_image atlas;
  struct owl_glyph glyphs[OWL_FONT_MAX_GLYPHS];
};

enum owl_code owl_font_init(struct owl_renderer *renderer, int size,
                            char const *path, struct owl_font *font);

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font);

#endif

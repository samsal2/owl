#ifndef OWL_FONT_H_
#define OWL_FONT_H_

#include "owl_renderer.h"
#include "owl_types.h"

#define OWL_FONT_GLYPHS_COUNT 128

struct owl_font_glyph {
  owl_i32 offset;
  owl_v2i advance;
  owl_v2i size;
  owl_v2i bearing;
};

struct owl_font {
  owl_i32 size;
  owl_i32 atlas_width;
  owl_i32 atlas_height;
  struct owl_renderer_image atlas;
  struct owl_font_glyph glyphs[OWL_FONT_GLYPHS_COUNT];
};

enum owl_code owl_font_init(struct owl_renderer *renderer, owl_i32 size,
                            char const *path, struct owl_font *font);

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font);

#endif
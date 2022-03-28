#ifndef OWL_FONT_H_
#define OWL_FONT_H_

#include "owl_renderer.h"
#include "owl_types.h"

#define OWL_FONT_FIRST_CHAR ((owl_i32)(' '))
#define OWL_FONT_CHAR_COUNT ((owl_i32)('~' - ' '))


struct owl_font_char {
  owl_u16 x0;
  owl_u16 y0;
  owl_u16 x1;
  owl_u16 y1;
  owl_v2 offset0;
  owl_v2 offset1;
  float x_advance;
};

struct owl_font_glyph {
  owl_v2 offset;
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

struct owl_font {
  owl_i32 size;
  owl_i32 atlas_width;
  owl_i32 atlas_height;
  struct owl_renderer_image atlas;
  struct owl_font_char chars[OWL_FONT_CHAR_COUNT];
};

enum owl_code owl_font_init(struct owl_renderer *renderer, owl_i32 size,
                            char const *path, struct owl_font *font);

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font);

enum owl_code owl_font_fill_glyph(struct owl_font const *font,
                                  char c,
                                  owl_v2 offset,
                                  struct owl_font_glyph *glyph);

#endif

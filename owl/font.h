#ifndef OWL_FONT_H_
#define OWL_FONT_H_

#include <owl/types.h>

#define OWL_GLYPH_COUNT 128

struct owl_renderer;

struct owl_glyph {
  int advance;
  OwlV2I p0;
  OwlV2I p1;
  OwlV2I offset;
};

struct owl_font {
  OwlTexture atlas;
  struct owl_glyph glyphs[OWL_GLYPH_COUNT];
};

enum owl_code owl_create_font(struct owl_renderer *renderer, int size,
                              char const *path, struct owl_font **font);

void owl_destroy_font(struct owl_renderer *renderer, struct owl_font *font);

#endif

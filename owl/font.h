#ifndef OWL_FONT_H_
#define OWL_FONT_H_

#include <owl/code.h>
#include <owl/fwd.h>
#include <owl/types.h>

#define OWL_GLYPH_COUNT 128

struct owl_glyph {
  int offset;
  OwlV2I advance;
  OwlV2I size;
  OwlV2I bearing;
};

struct owl_font {
  int size;
  int atlas_width;
  int atlas_height;
  OwlTexture atlas;
  struct owl_glyph glyphs[OWL_GLYPH_COUNT];
};

enum owl_code owl_create_font(struct owl_renderer *renderer, int size,
                              char const *path, struct owl_font **font);

void owl_destroy_font(struct owl_renderer *renderer, struct owl_font *font);

enum owl_code owl_submit_text_group(struct owl_renderer *renderer,
                                    struct owl_font const *font,
                                    OwlV2 const pos, OwlV3 const color,
                                    char const *text);

#endif

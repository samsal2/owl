#ifndef OWL_SOURCE_FONT_H_
#define OWL_SOURCE_FONT_H_

#include "../include/owl/types.h"

#include "img.h"

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
  struct owl_img atlas;
  struct owl_glyph glyphs[OWL_GLYPH_COUNT];
};

#endif

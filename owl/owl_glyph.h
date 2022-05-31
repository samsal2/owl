#ifndef OWL_GLYPH_H_
#define OWL_GLYPH_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

struct owl_packed_char
{
  owl_u16 x0;
  owl_u16 y0;
  owl_u16 x1;
  owl_u16 y1;
  float   x_offset;
  float   y_offset;
  float   x_advance;
  float   x_offset2;
  float   y_offset2;
};

struct owl_glyph
{
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

OWL_END_DECLS

#endif

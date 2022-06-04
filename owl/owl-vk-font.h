#ifndef OWL_VK_FONT_H_
#define OWL_VK_FONT_H_

#include "owl-definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

struct owl_vk_packed_char {
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
  float x_offset;
  float y_offset;
  float x_advance;
  float x_offset2;
  float y_offset2;
};

struct owl_vk_glyph {
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

owl_public owl_code
owl_vk_font_load(struct owl_vk_renderer *vk, uint64_t size, char const *path);

owl_public void
owl_vk_font_unload(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_font_fill_glyph(struct owl_vk_renderer *vk, char c, owl_v2 offset,
                       struct owl_vk_glyph *glyph);

OWL_END_DECLS

#endif

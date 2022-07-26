#ifndef OWL_FONT_H
#define OWL_FONT_H

#include "owl_definitions.h"
#include "owl_texture.h"

OWL_BEGIN_DECLARATIONS

struct owl_renderer;

#define OWL_FIRST_CHAR ((int)(' '))
#define OWL_NUM_CHARS ((int)('~' - ' '))

struct owl_packed_char {
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

struct owl_glyph {
	owl_v3 positions[4];
	owl_v2 uvs[4];
};

struct owl_font {
	struct owl_texture atlas;
	struct owl_packed_char chars[OWL_NUM_CHARS];
};

OWLAPI int owl_font_init(struct owl_renderer *r, char const *path, int32_t size,
			 struct owl_font *font);

OWLAPI void owl_font_deinit(struct owl_renderer *r, struct owl_font *font);

OWLAPI int owl_font_fill_glyph(struct owl_font *font, char c, owl_v2 offset,
			       int32_t w, int32_t h, struct owl_glyph *glyph);

OWL_END_DECLARATIONS

#endif

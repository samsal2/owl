#include "owl_font.h"

#include "owl_draw_command.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_types.h"
#include "owl_vector_math.h"
#include "stb_truetype.h"

#include <stdio.h>

enum owl_code owl_font_load_file(char const *path, owl_byte **data) {
  owl_u64 sz;
  FILE *file;

  enum owl_code code = OWL_SUCCESS;

  if (!(file = fopen(path, "rb"))) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  fseek(file, 0, SEEK_END);

  sz = ftell(file);

  fseek(file, 0, SEEK_SET);

  if (!(*data = OWL_MALLOC(sz))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out_close_file;
  }

  fread(*data, sz, 1, file);

out_close_file:
  fclose(file);

out:
  return code;
}

void owl_font_unload_file(owl_byte *data) { OWL_FREE(data); }

#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_WIDTH * OWL_FONT_ATLAS_HEIGHT)

#define OWL_FONT_FIRST_CHAR ((owl_i32)(' '))
#define OWL_FONT_CHAR_COUNT ((owl_i32)('~' - ' '))

OWL_STATIC_ASSERT(
    sizeof(struct owl_font_packed_char) == sizeof(stbtt_packedchar),
    "owl_font_char and stbtt_packedchar must represent the same struct");

enum owl_code owl_font_init(struct owl_renderer *r, owl_i32 size,
                            char const *path, struct owl_font *font) {
  owl_byte *file;
  owl_byte *bitmap;
  stbtt_pack_context pack_context;
  struct owl_renderer_image_init_desc image_desc;

  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_font_load_file(path, &file))) {
    goto out;
  }

  if (!(bitmap = OWL_CALLOC(OWL_FONT_ATLAS_SIZE, sizeof(owl_byte)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out_unload_file;
  }

  if (!stbtt_PackBegin(&pack_context, bitmap, OWL_FONT_ATLAS_WIDTH,
                       OWL_FONT_ATLAS_HEIGHT, 0, 1, NULL)) {
    code = OWL_ERROR_UNKNOWN;
    goto out_free_bitmap;
  }

  /* FIXME(samuel): hardcoded */
  stbtt_PackSetOversampling(&pack_context, 2, 2);

  /* HACK(samuel): idk if it's legal to alias a different type with the exact
   * same layout, but it _works_ so ill leave it at that*/
  if (!stbtt_PackFontRange(&pack_context, file, 0, size, OWL_FONT_FIRST_CHAR,
                           OWL_FONT_CHAR_COUNT,
                           (stbtt_packedchar *)(&font->packed_chars[0]))) {
    code = OWL_ERROR_UNKNOWN;
    goto out_end_pack;
  }

  image_desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_DATA;
  image_desc.src_path = NULL;
  image_desc.src_data = bitmap;
  image_desc.src_data_width = OWL_FONT_ATLAS_WIDTH;
  image_desc.src_data_height = OWL_FONT_ATLAS_HEIGHT;
  image_desc.src_data_pixel_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  image_desc.sampler_use_default = 0;
  image_desc.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST;
  image_desc.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_desc.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_desc.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_desc.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_desc.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;

  code = owl_renderer_image_init(r, &image_desc, &font->atlas);

  if (OWL_SUCCESS != code) {
    goto out_end_pack;
  }

out_end_pack:
  stbtt_PackEnd(&pack_context);

out_free_bitmap:
  OWL_FREE(bitmap);

out_unload_file:
  owl_font_unload_file(file);

out:
  return code;
}

void owl_font_deinit(struct owl_renderer *r, struct owl_font *f) {
  owl_renderer_image_deinit(r, &f->atlas);
}

enum owl_code owl_font_fill_glyph(struct owl_font const *f, char c,
                                  owl_v2 offset, struct owl_font_glyph *glyph) {
  stbtt_aligned_quad quad;
  enum owl_code code = OWL_SUCCESS;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&f->packed_chars[0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_FONT_FIRST_CHAR, &offset[0], &offset[1], &quad,
                      1);

  OWL_V3_SET(quad.x0, quad.y0, 0.0F, glyph->positions[0]);
  OWL_V3_SET(quad.x1, quad.y0, 0.0F, glyph->positions[1]);
  OWL_V3_SET(quad.x0, quad.y1, 0.0F, glyph->positions[2]);
  OWL_V3_SET(quad.x1, quad.y1, 0.0F, glyph->positions[3]);

  OWL_V2_SET(quad.s0, quad.t0, glyph->uvs[0]);
  OWL_V2_SET(quad.s1, quad.t0, glyph->uvs[1]);
  OWL_V2_SET(quad.s0, quad.t1, glyph->uvs[2]);
  OWL_V2_SET(quad.s1, quad.t1, glyph->uvs[3]);

  return code;
}

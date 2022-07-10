#include "owl_font.h"

#include "owl_internal.h"
#include "owl_plataform.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#include "stb_truetype.h"

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_HEIGHT * OWL_FONT_ATLAS_WIDTH)

OWLAPI int owl_font_init(struct owl_renderer *r, char const *path,
                         int32_t size, struct owl_font *font) {

  int ret;
  uint8_t *bitmap;
  stbtt_pack_context pack;
  stbtt_packedchar *packed_chars;
  struct owl_plataform_file file;
  struct owl_texture_desc texture_desc;
  int32_t stb_result;
  int32_t const width = OWL_FONT_ATLAS_WIDTH;
  int32_t const height = OWL_FONT_ATLAS_HEIGHT;
  int32_t const first = OWL_FIRST_CHAR;
  int32_t const num = OWL_NUM_CHARS;

  if (r->font_loaded)
    owl_renderer_unload_font(r);

  ret = owl_plataform_load_file(path, &file);
  if (ret)
    return ret;

  bitmap = OWL_CALLOC(OWL_FONT_ATLAS_SIZE, sizeof(uint8_t));
  if (!bitmap) {
    ret = OWL_ERROR_NO_MEMORY;
    goto error_unload_file;
  }

  stb_result = stbtt_PackBegin(&pack, bitmap, width, height, 0, 1, NULL);
  if (!stb_result) {
    ret = OWL_ERROR_FATAL;
    goto error_free_bitmap;
  }

  stbtt_PackSetOversampling(&pack, 2, 2);

  packed_chars = (stbtt_packedchar *)(&font->chars[0]);

  stb_result = stbtt_PackFontRange(&pack, file.data, 0, size, first, num,
                                   packed_chars);
  if (!stb_result) {
    ret = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  texture_desc.source = OWL_TEXTURE_SOURCE_DATA;
  texture_desc.type = OWL_TEXTURE_TYPE_2D;
  texture_desc.path = NULL;
  texture_desc.pixels = bitmap;
  texture_desc.width = OWL_FONT_ATLAS_WIDTH;
  texture_desc.height = OWL_FONT_ATLAS_HEIGHT;
  texture_desc.format = OWL_R8_UNORM;

  ret = owl_texture_init(r, &texture_desc, &font->atlas);
  if (!ret)
    r->font_loaded = 1;

error_pack_end:
  stbtt_PackEnd(&pack);

error_free_bitmap:
  OWL_FREE(bitmap);

error_unload_file:
  owl_plataform_unload_file(&file);

  return ret;
}

OWLAPI void owl_font_deinit(struct owl_renderer *r, struct owl_font *font) {
  owl_texture_deinit(r, &font->atlas);
}

OWLAPI int owl_font_fill_glyph(struct owl_font *font, char c, owl_v2 offset,
                               int32_t w, int32_t h, struct owl_glyph *glyph) {
  stbtt_aligned_quad quad;
  int const atlas_width = OWL_FONT_ATLAS_WIDTH;
  int const atlas_height = OWL_FONT_ATLAS_HEIGHT;
  stbtt_packedchar *packed_chars = (stbtt_packedchar *)(&font->chars[0]);
  int ret = OWL_OK;

  /* -1 ---- +
   *  |      |  h
   *  |      |
   *  + ---- 1
   *     w      */

  stbtt_GetPackedQuad(packed_chars, atlas_width, atlas_height,
                      c - OWL_FIRST_CHAR, &offset[0], &offset[1], &quad, 1);

  OWL_V3_SET(glyph->positions[0], quad.x0 / (float)w, quad.y0 / (float)h,
             0.0F);
  OWL_V3_SET(glyph->positions[1], quad.x1 / (float)w, quad.y0 / (float)h,
             0.0F);
  OWL_V3_SET(glyph->positions[2], quad.x0 / (float)w, quad.y1 / (float)h,
             0.0F);
  OWL_V3_SET(glyph->positions[3], quad.x1 / (float)w, quad.y1 / (float)h,
             0.0F);

  OWL_V2_SET(glyph->uvs[0], quad.s0, quad.t0);
  OWL_V2_SET(glyph->uvs[1], quad.s1, quad.t0);
  OWL_V2_SET(glyph->uvs[2], quad.s0, quad.t1);
  OWL_V2_SET(glyph->uvs[3], quad.s1, quad.t1);

  return ret;
}

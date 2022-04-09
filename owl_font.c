#include "owl_font.h"

#include "owl_draw_command.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_types.h"
#include "owl_vector_math.h"

#include "stb_truetype.h"

#include <stdio.h>

#if 0
/* clang-format off */
#include <ft2build.h>
#include FT_FREETYPE_H
/* clang-format on */

#define OWL_FIRST_CHAR 32
#define OWL_FONT_X_OFFSET_PAD 2

OWL_GLOBAL owl_i32 g_ft_library_reference_count = 0;
OWL_GLOBAL FT_Library g_ft_library;

OWL_INTERNAL void owl_ensure_ft_library_(void) {
  FT_Error err = FT_Err_Ok;

  if (!(g_ft_library_reference_count++))
    err = FT_Init_FreeType(&g_ft_library);

  OWL_UNUSED(err);
  OWL_ASSERT(FT_Err_Ok == err);
}

OWL_INTERNAL void owl_decrement_ft_library_count_(void) {
  OWL_ASSERT(g_ft_library_reference_count);

  if (!--g_ft_library_reference_count)
    FT_Done_FreeType(g_ft_library);
}

OWL_INTERNAL void owl_font_calc_dims_(FT_Face face, owl_i32 *width,
                                      owl_i32 *height) {
  owl_i32 i;

  *width = 0;
  *height = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_GLYPHS_COUNT; ++i) {
    FT_Load_Char(face, (owl_u32)i, FT_LOAD_BITMAP_METRICS_ONLY);

    *width += (int)face->glyph->bitmap.width + OWL_FONT_X_OFFSET_PAD;
    *height = OWL_MAX(*height, (int)face->glyph->bitmap.rows);
  }
}

OWL_INTERNAL void owl_face_glyph_bitmap_copy_(FT_Face face, owl_i32 x_offset,
                                              owl_i32 atlas_width,
                                              owl_i32 atlas_height,
                                              owl_byte *data) {
  owl_i32 bx; /* bitmap x position */

  OWL_UNUSED(atlas_height);

  for (bx = 0; bx < (int)face->glyph->bitmap.width; ++bx) {
    owl_i32 by;                       /* bitmap y position, shared by data*/
    owl_i32 const dx = x_offset + bx; /* atlas x position */

    for (by = 0; by < (int)face->glyph->bitmap.rows; ++by) {
      owl_i32 const dw = atlas_width;                    /* data width */
      owl_i32 const bw = (int)face->glyph->bitmap.width; /* buf width */

      data[by * dw + dx] = face->glyph->bitmap.buffer[by * bw + bx];
    }
  }
}

OWL_INTERNAL enum owl_code owl_font_init_atlas_(struct owl_renderer *renderer,
                                                owl_byte const *data,
                                                struct owl_font *font) {
  enum owl_code code = OWL_SUCCESS;
  struct owl_renderer_image_init_desc image_desc;

  image_desc.source_type = OWL_RENDERER_IMAGE_SOURCE_TYPE_DATA;
  image_desc.source_data = data;
  image_desc.source_data_width = font->atlas_width;
  image_desc.source_data_height = font->atlas_height;
  image_desc.source_data_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  image_desc.use_default_sampler = 0;
  image_desc.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR;
  image_desc.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_LINEAR;
  image_desc.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_LINEAR;
  image_desc.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  image_desc.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  image_desc.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;

  code = owl_renderer_init_image(renderer, &desc, &font->atlas);

  return code;
}

enum owl_code owl_font_init(struct owl_renderer *renderer, owl_i32 size,
                            char const *path, struct owl_font *font) {
  owl_i32 i;
  owl_i32 x;
  owl_byte *data;
  FT_Face face;
  enum owl_code code = OWL_SUCCESS;

  owl_ensure_ft_library_();

  font->size = size;

  if (FT_Err_Ok != (FT_New_Face(g_ft_library, path, 0, &face))) {
    code = OWL_ERROR_BAD_INIT;
    goto end;
  }

  if (FT_Err_Ok != FT_Set_Char_Size(face, 0, size << 6, 96, 96)) {
    code = OWL_ERROR_BAD_INIT;
    goto end_err_done_face;
  }

  owl_font_calc_dims_(face, &font->atlas_width, &font->atlas_height);

  data = OWL_CALLOC((owl_u64)(font->atlas_width * font->atlas_height),
                    sizeof(owl_u8));

  if (!data) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_err_done_face;
  }

  x = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_GLYPHS_COUNT; ++i) {
    if (FT_Err_Ok != FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER)) {
      code = OWL_ERROR_UNKNOWN;
      goto end_err_free_data;
    }

    owl_face_glyph_bitmap_copy_(face, x, font->atlas_width, font->atlas_height,
                                data);

    /* set the current glyph data */
    font->glyphs[i].offset = x;
    font->glyphs[i].advance[0] = (int)face->glyph->advance.x >> 6;
    font->glyphs[i].advance[1] = (int)face->glyph->advance.y >> 6;

    font->glyphs[i].size[0] = (int)face->glyph->bitmap.width;
    font->glyphs[i].size[1] = (int)face->glyph->bitmap.rows;

    font->glyphs[i].bearing[0] = face->glyph->bitmap_left;
    font->glyphs[i].bearing[1] = face->glyph->bitmap_top;

    /* HACK(samuel): add 2 to avoid bleeding */
    x += (int)face->glyph->bitmap.width + OWL_FONT_X_OFFSET_PAD;
  }

  code = owl_font_init_atlas_(renderer, data, font);

  if (OWL_SUCCESS != code)
    goto end_err_free_data;

end_err_free_data:
  OWL_FREE(data);

end_err_done_face:
  FT_Done_Face(face);

end:
  return code;
}

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font) {
  owl_renderer_deinit_image(renderer, &font->atlas);
  owl_decrement_ft_library_count_();
}
#else

enum owl_code owl_font_load_file_(char const *path, owl_byte **data) {
  owl_u64 size;
  FILE *file;

  enum owl_code code = OWL_SUCCESS;

  if (!(file = fopen(path, "rb"))) {
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  fseek(file, 0, SEEK_END);

  size = ftell(file);

  fseek(file, 0, SEEK_SET);

  if (!(*data = OWL_MALLOC(size))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_close_file;
  }

  fread(*data, size, 1, file);

end_close_file:
  fclose(file);

end:
  return code;
}

void owl_font_unload_file_(owl_byte *data) { OWL_FREE(data); }

#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_WIDTH * OWL_FONT_ATLAS_HEIGHT)

#define OWL_FONT_FIRST_CHAR ((owl_i32)(' '))
#define OWL_FONT_CHAR_COUNT ((owl_i32)('~' - ' '))

OWL_STATIC_ASSERT(
    sizeof(struct owl_font_packed_char) == sizeof(stbtt_packedchar),
    "owl_font_char and stbtt_packedchar must represent the same struct");

enum owl_code owl_font_init(struct owl_renderer *renderer, owl_i32 size,
                            char const *path, struct owl_font *font) {
  owl_byte *font_file_data;
  owl_byte *font_bitmap_data;
  stbtt_pack_context pack_context;
  struct owl_renderer_image_init_desc image_init_desc;

  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_font_load_file_(path, &font_file_data)))
    goto end;

  if (!(font_bitmap_data = OWL_CALLOC(OWL_FONT_ATLAS_SIZE, sizeof(owl_byte)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_unload_file;
  }

  if (!stbtt_PackBegin(&pack_context, font_bitmap_data, OWL_FONT_ATLAS_WIDTH,
                       OWL_FONT_ATLAS_HEIGHT, 0, 1, NULL)) {
    code = OWL_ERROR_UNKNOWN;
    goto end_free_font_bitmap_data;
  }

  /* FIXME(samuel): hardcoded */
  stbtt_PackSetOversampling(&pack_context, 2, 2);

  /* HACK(samuel): idk if it's legal to alias a different type with the exact
   * same layout, but "it works" so ill leave it at that*/
  if (!stbtt_PackFontRange(&pack_context, font_file_data, 0, size,
                           OWL_FONT_FIRST_CHAR, OWL_FONT_CHAR_COUNT,
                           (stbtt_packedchar *)(&font->packed_chars[0]))) {
    code = OWL_ERROR_UNKNOWN;
    goto end_pack;
  }

  image_init_desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_DATA;
  image_init_desc.src_path = NULL;
  image_init_desc.src_data = font_bitmap_data;
  image_init_desc.src_data_width = OWL_FONT_ATLAS_WIDTH;
  image_init_desc.src_data_height = OWL_FONT_ATLAS_HEIGHT;
  image_init_desc.src_data_pixel_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  image_init_desc.use_default_sampler = 0;
  image_init_desc.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR;
  image_init_desc.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_init_desc.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_init_desc.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_init_desc.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_init_desc.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;

  code = owl_renderer_init_image(renderer, &image_init_desc, &font->atlas);

  if (OWL_SUCCESS != code)
    goto end_pack;

end_pack:
  stbtt_PackEnd(&pack_context);

end_free_font_bitmap_data:
  OWL_FREE(font_bitmap_data);

end_unload_file:
  owl_font_unload_file_(font_file_data);

end:
  return code;
}

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font) {
  owl_renderer_deinit_image(renderer, &font->atlas);
}

enum owl_code owl_font_fill_glyph(struct owl_font const *font, char c,
                                  owl_v2 offset, struct owl_font_glyph *glyph) {
  stbtt_aligned_quad quad;
  enum owl_code code = OWL_SUCCESS;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&font->packed_chars[0]),
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

#endif

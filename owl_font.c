#include "owl_font.h"

#include "owl_draw_command.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_types.h"
#include "owl_vector_math.h"

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
  struct owl_renderer_image_init_desc desc;

  desc.source_type = OWL_RENDERER_IMAGE_SOURCE_TYPE_DATA;
  desc.source_data = data;
  desc.source_data_width = font->atlas_width;
  desc.source_data_height = font->atlas_height;
  desc.source_data_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  desc.use_default_sampler = 0;
  desc.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR;
  desc.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_LINEAR;
  desc.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_LINEAR;
  desc.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  desc.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  desc.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;

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

enum owl_code owl_font_init(struct owl_renderer *renderer, owl_i32 size,
                            char const *path, struct owl_font *font) {
  enum owl_code code = OWL_SUCCESS;

  OWL_MEMSET(font, 0, sizeof(*font));
  OWL_UNUSED(renderer);
  OWL_UNUSED(size);
  OWL_UNUSED(path);
  OWL_UNUSED(font);

  return code;
}

void owl_font_deinit(struct owl_renderer *renderer, struct owl_font *font) {
  OWL_UNUSED(renderer);
  OWL_UNUSED(font);
}

#endif

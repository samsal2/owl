#include "font.h"

#include "draw.h"
#include "internal.h"
#include "renderer.h"
#include "types.h"
#include "vector_math.h"

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
  owl_i32i;

  *width = 0;
  *height = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_GLYPHS_COUNT; ++i) {
    FT_Load_Char(face, (unsigned)i, FT_LOAD_BITMAP_METRICS_ONLY);

    *width += (int)face->glyph->bitmap.width + OWL_FONT_X_OFFSET_PAD;
    *height = OWL_MAX(*height, (int)face->glyph->bitmap.rows);
  }
}

OWL_INTERNAL void owl_face_glyph_bitmap_copy_(FT_Face face, owl_i32x_offset,
                                              owl_i32atlas_width,
                                              owl_i32atlas_height,
                                              owl_byte *data) {
  owl_i32bx; /* bitmap x position */

  OWL_UNUSED(atlas_height);

  for (bx = 0; bx < (int)face->glyph->bitmap.width; ++bx) {
    owl_i32by;                       /* bitmap y position, shared by data*/
    owl_i32const dx = x_offset + bx; /* atlas x position */

    for (by = 0; by < (int)face->glyph->bitmap.rows; ++by) {
      owl_i32const dw = atlas_width;                    /* data width */
      owl_i32const bw = (int)face->glyph->bitmap.width; /* buf width */

      data[by * dw + dx] = face->glyph->bitmap.buffer[by * bw + bx];
    }
  }
}

OWL_INTERNAL enum owl_code owl_font_init_atlas_(struct owl_renderer *r,
                                                owl_byte const *data,
                                                struct owl_font *font) {
  enum owl_code code = OWL_SUCCESS;
  struct owl_image_init_info iii;

  iii.src_type = OWL_IMAGE_INIT_INFO_SRC_TYPE_DATA;
  iii.src_storage.as_data.data = data;
  iii.src_storage.as_data.width = font->atlas_width;
  iii.src_storage.as_data.height = font->atlas_height;
  iii.src_storage.as_data.format = OWL_PIXEL_FORMAT_R8_UNORM;
  iii.sampler_type = OWL_IMAGE_INIT_INFO_SAMPLER_TYPE_SPECIFY;
  iii.sampler_storage.as_specify.mip_mode = OWL_SAMPLER_MIP_MODE_LINEAR;
  iii.sampler_storage.as_specify.min_filter = OWL_SAMPLER_FILTER_LINEAR;
  iii.sampler_storage.as_specify.mag_filter = OWL_SAMPLER_FILTER_LINEAR;
  iii.sampler_storage.as_specify.wrap_u = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  iii.sampler_storage.as_specify.wrap_v = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  iii.sampler_storage.as_specify.wrap_w = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;

  code = owl_image_init(r, &iii, &font->atlas);

  return code;
}

enum owl_code owl_font_init(struct owl_renderer *r, owl_i32size,
                            char const *path, struct owl_font *font) {
  owl_i32i;
  owl_i32x;
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

  code = owl_font_init_atlas_(r, data, font);

  if (OWL_SUCCESS != code)
    goto end_err_free_data;

end_err_free_data:
  OWL_FREE(data);

end_err_done_face:
  FT_Done_Face(face);

end:
  return code;
}

void owl_font_deinit(struct owl_renderer *r, struct owl_font *font) {
  owl_image_deinit(r, &font->atlas);
  owl_decrement_ft_library_count_();
}

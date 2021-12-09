#include <ft2build.h>
#include <owl/font.h>
#include <owl/internal.h>
#include <owl/texture.h>
#include FT_FREETYPE_H
#include <math.h>

OWL_INTERNAL void owl_calc_dims_(FT_Face face, int *width, int *height) {
  float const sqr = sqrtf((float)OWL_GLYPH_COUNT);
  float const ceil = ceilf(sqr);
  int dim = (1 + ((int)face->size->metrics.height >> 6)) * (int)ceil;

  *width = 1;

  while (*width < dim)
    *width <<= 1;

  *height = *width;
}

#define OWL_LOAD_FLAGS                                                       \
  (FT_LOAD_RENDER | FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT)

enum owl_code owl_create_font(struct owl_renderer *renderer, int size,
                              char const *path, struct owl_font **font) {
  int i;
  int x;
  int y;
  int width;
  int height;
  OwlByte *data;
  FT_Library ft;
  FT_Face face;
  enum owl_code err = OWL_SUCCESS;

  if (!(*font = OWL_MALLOC(sizeof(**font)))) {
    err = OWL_ERROR_UNKNOWN;
    goto end;
  }

  if (FT_Err_Ok != (FT_Init_FreeType(&ft))) {
    err = OWL_ERROR_UNKNOWN;
    goto end_err_free_font;
  }

  if (FT_Err_Ok != (FT_New_Face(ft, path, 0, &face))) {
    err = OWL_ERROR_UNKNOWN;
    goto end_err_done_ft;
  }

  if (FT_Err_Ok != FT_Set_Char_Size(face, 0, size << 6, 96, 96)) {
    err = OWL_ERROR_UNKNOWN;
    goto end_err_done_face;
  }

  owl_calc_dims_(face, &width, &height);

  if (!(data = OWL_CALLOC((unsigned)(width * height), sizeof(*data)))) {
    err = OWL_ERROR_UNKNOWN;
    goto end_err_done_face;
  }

  x = 0;
  y = 0;

  for (i = 0; i < OWL_GLYPH_COUNT; ++i) {
    int row;
    FT_Bitmap *bm;

    if (FT_Err_Ok != FT_Load_Char(face, (unsigned)i, OWL_LOAD_FLAGS)) {
      err = OWL_ERROR_UNKNOWN;
      goto end_err_free_data;
    }

    bm = &face->glyph->bitmap;

    if (x + (int)bm->width >= width) {
      x = 0;
      y += ((int)(face->size->metrics.height >> 6) + 1);
    }

    for (row = 0; row < (int)bm->rows; ++row) {
      int col;
      for (col = 0; col < (int)bm->width; ++col) {
        data[(y + row) * width + (x + col)] =
            bm->buffer[row * bm->pitch + col];
      }
    }

    (*font)->glyphs[i].p0[0] = x;
    (*font)->glyphs[i].p0[1] = y;
    (*font)->glyphs[i].p1[0] = x + (int)bm->width;
    (*font)->glyphs[i].p1[1] = y + (int)bm->rows;
    (*font)->glyphs[i].offset[0] = face->glyph->bitmap_left;
    (*font)->glyphs[i].offset[1] = face->glyph->bitmap_top;
    (*font)->glyphs[i].advance = (int)face->glyph->advance.x >> 6;

    x += (int)bm->width + 1;
  }

  if (OWL_SUCCESS !=
      (err = owl_create_texture(renderer, width, height, data,
                                OWL_PIXEL_FORMAT_R8_UNORM, &(*font)->atlas)))
    goto end_err_free_data;

  OWL_FREE(data);
  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  goto end;

end_err_free_data:
  OWL_FREE(data);

end_err_done_face:
  FT_Done_Face(face);

end_err_done_ft:
  FT_Done_FreeType(ft);

end_err_free_font:
  OWL_FREE(*font);

end:
  return err;
}

void owl_destroy_font(struct owl_renderer *renderer, struct owl_font *font) {
  owl_destroy_texture(renderer, font->atlas);
  OWL_FREE(font);
}

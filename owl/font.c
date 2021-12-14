#include <owl/memory.h>
#include <math.h>
#include <owl/font.h>
#include <owl/internal.h>
#include <owl/math.h>
#include <owl/render_group.h>
#include <owl/renderer.inl>
#include <owl/texture.h>
#include <owl/texture.inl>

/* clang-format off */
#include <ft2build.h>
#include FT_FREETYPE_H
/* clang-format on */

#define OWL_FIRST_CHAR 32

OWL_INTERNAL void owl_calc_dims_(FT_Face face, int *width, int *height) {
  int i;

  *width = 0;
  *height = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_GLYPH_COUNT; ++i) {
    /* FIXME(samuel): lets close our eyes and pretend this can't fail! */
    FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER);

    *width += (int)face->glyph->bitmap.width;
    *height = OWL_MAX(*height, (int)face->glyph->bitmap.rows);
  }
}

enum owl_code owl_create_font(struct owl_renderer *renderer, int size,
                              char const *path, struct owl_font **font) {
  int i;
  int x;
  OwlByte *data;
  FT_Library ft;
  FT_Face face;
  struct owl_tmp_submit_mem_ref ref;
  enum owl_code err = OWL_SUCCESS;

  /* TODO(samuel): chances are im only going to be creating one font,
     could do something analogous to owl_vk_texture_manager */
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

  owl_calc_dims_(face, &(*font)->width, &(*font)->height);

  if (!(data = owl_alloc_tmp_submit_mem(
            renderer, (unsigned)((*font)->width * (*font)->height), &ref))) {
    err = OWL_ERROR_UNKNOWN;
    goto end_err_done_face;
  }

  x = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_GLYPH_COUNT; ++i) {
    if (FT_Err_Ok != FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER)) {
      err = OWL_ERROR_UNKNOWN;
      goto end_err_done_face;
    }

    /* copy the bitmap into the buffer */
    {
      int bx; /* buffer x position */
      for (bx = 0; bx < (int)face->glyph->bitmap.width; ++bx) {
        int by;                /* buffer y position, shared by data*/
        int const dx = x + bx; /* data x position */

        for (by = 0; by < (int)face->glyph->bitmap.rows; ++by) {
          int const dwidth = (*font)->width;                 /* data width */
          int const bwidth = (int)face->glyph->bitmap.width; /* buf width */

          data[by * dwidth + dx] =
              face->glyph->bitmap.buffer[by * bwidth + bx];
        }
      }
    }

    (*font)->glyphs[i].offset = x;
    (*font)->glyphs[i].advance[0] = (int)face->glyph->advance.x >> 6;
    (*font)->glyphs[i].advance[1] = (int)face->glyph->advance.y >> 6;

    (*font)->glyphs[i].size[0] = (int)face->glyph->bitmap.width;
    (*font)->glyphs[i].size[1] = (int)face->glyph->bitmap.rows;

    (*font)->glyphs[i].bearing[0] = face->glyph->bitmap_left;
    (*font)->glyphs[i].bearing[1] = face->glyph->bitmap_top;

    x += (int)face->glyph->bitmap.width;
  }

  (*font)->size = size;

  if (OWL_SUCCESS != (err = owl_create_texture_with_ref(
                          renderer, (*font)->width, (*font)->height, &ref,
                          OWL_PIXEL_FORMAT_R8_UNORM, OWL_SAMPLER_TYPE_LINEAR,
                          &(*font)->atlas)))

    goto end_err_done_face;

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  goto end;

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

void owl_submit_text_group(struct owl_renderer *renderer,
                           struct owl_font const *font, OwlV2 const pos,
                           OwlV3 const color, char const *text) {
  char const *c;
  OwlV2 cpos; /* move the position to a copy */
  OWL_COPY_V2(pos, cpos);

  /* TODO(samuel): cleanup */
  for (c = text; '\0' != *c; ++c) {
    struct owl_vertex *v;
    struct owl_render_group group;
    struct owl_glyph const *glyph = &font->glyphs[(int)*c];

    /* FIXME(samuel): using renderer extent, which in macos it's not 1:1 with
     * the window size */
    float const sx =
        cpos[0] + (float)glyph->bearing[0] / (float)renderer->extent.width;

    float const sy =
        cpos[1] - (float)glyph->bearing[1] / (float)renderer->extent.height;

    float const cw = (float)glyph->size[0] / (float)renderer->extent.width;
    float const ch = (float)glyph->size[1] / (float)renderer->extent.height;

    float const tx = (float)glyph->offset / (float)font->width;

    float const tw = (float)glyph->size[0] / (float)font->width;
    float const th = (float)glyph->size[1] / (float)font->height;

    group.type = OWL_RENDER_GROUP_TYPE_QUAD;
    group.storage.as_quad.texture = font->atlas;

    OWL_IDENTITY_M4(group.storage.as_quad.pvm.proj);
    OWL_IDENTITY_M4(group.storage.as_quad.pvm.model);
    OWL_IDENTITY_M4(group.storage.as_quad.pvm.view);

    v = &group.storage.as_quad.vertex[0];
    OWL_SET_V3(sx, sy, 0.0F, v->pos);
    OWL_COPY_V3(color, v->color);
    OWL_SET_V2(tx, 0.0F, v->uv);

    v = &group.storage.as_quad.vertex[1];
    OWL_SET_V3(sx + cw, sy, 0.0F, v->pos);
    OWL_COPY_V3(color, v->color);
    OWL_SET_V2(tx + tw, 0.0F, v->uv);

    v = &group.storage.as_quad.vertex[2];
    OWL_SET_V3(sx, sy + ch, 0.0F, v->pos);
    OWL_COPY_V3(color, v->color);
    OWL_SET_V2(tx, th, v->uv);

    v = &group.storage.as_quad.vertex[3];
    OWL_SET_V3(sx + cw, sy + ch, 0.0F, v->pos);
    OWL_COPY_V3(color, v->color);
    OWL_SET_V2(tx + tw, th, v->uv);

    owl_submit_render_group(renderer, &group);

    cpos[0] += glyph->advance[0] / (float)renderer->extent.width;

    /* FIXME(samuel): not sure if i should substract or add */
    cpos[1] += glyph->advance[1] / (float)renderer->extent.height;
  }
}

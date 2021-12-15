#include "owl/types.h"
#include <math.h>
#include <owl/font.h>
#include <owl/internal.h>
#include <owl/math.h>
#include <owl/memory.h>
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

  owl_calc_dims_(face, &(*font)->atlas_width, &(*font)->atlas_height);

  if (!(data = owl_alloc_tmp_submit_mem(
            renderer,
            (unsigned)((*font)->atlas_width * (*font)->atlas_height),
            &ref))) {
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
          int const dw = (*font)->atlas_width;           /* data width */
          int const bw = (int)face->glyph->bitmap.width; /* buf width */

          data[by * dw + dx] = face->glyph->bitmap.buffer[by * bw + bx];
        }
      }
    }

    /* set the current glyph data */
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

  if (OWL_SUCCESS !=
      (err = owl_create_texture_with_ref(
           renderer, (*font)->atlas_width, (*font)->atlas_height, &ref,
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

OWL_INTERNAL enum owl_code
owl_fill_char_quad(struct owl_extent const *window,
                   struct owl_font const *font, OwlV2 const pos,
                   OwlV3 const color, char c,
                   struct owl_render_group *group) {
  float tex_off;        /* texture offset in texture coords*/
  OwlV2 fixed_pos;      /* position taking into account the bearing in screen
                           coords*/
  OwlV2 glyph_scr_size; /* char size in screen coords */
  OwlV2 glyph_tex_size; /* uv without the offset in texture coords*/

  struct owl_vertex *v;
  struct owl_glyph const *g = &font->glyphs[(int)c];

  enum owl_code err = OWL_SUCCESS;

  if ((int)c >= OWL_ARRAY_SIZE(font->glyphs)) {
    err = OWL_ERROR_UNKNOWN;
    goto end;
  }

  fixed_pos[0] = pos[0] + (float)g->bearing[0] / (float)window->width;
  fixed_pos[1] = pos[1] - (float)g->bearing[1] / (float)window->height;

  glyph_scr_size[0] = (float)g->size[0] / (float)window->width;
  glyph_scr_size[1] = (float)g->size[1] / (float)window->height;

  tex_off = (float)g->offset / (float)font->atlas_width;

  glyph_tex_size[0] = (float)g->size[0] / (float)font->atlas_width;
  glyph_tex_size[1] = (float)g->size[1] / (float)font->atlas_height;

  group->type = OWL_RENDER_GROUP_TYPE_QUAD;
  group->storage.as_quad.texture = font->atlas;

  OWL_IDENTITY_M4(group->storage.as_quad.pvm.proj);
  OWL_IDENTITY_M4(group->storage.as_quad.pvm.model);
  OWL_IDENTITY_M4(group->storage.as_quad.pvm.view);

  v = &group->storage.as_quad.vertex[0];
  v->pos[0] = fixed_pos[0];
  v->pos[1] = fixed_pos[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = tex_off;
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertex[1];
  v->pos[0] = fixed_pos[0] + glyph_scr_size[0];
  v->pos[1] = fixed_pos[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = tex_off + glyph_tex_size[0];
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertex[2];
  v->pos[0] = fixed_pos[0];
  v->pos[1] = fixed_pos[1] + glyph_scr_size[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = tex_off;
  v->uv[1] = glyph_tex_size[1];

  v = &group->storage.as_quad.vertex[3];
  v->pos[0] = fixed_pos[0] + glyph_scr_size[0];
  v->pos[1] = fixed_pos[1] + glyph_scr_size[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = tex_off + glyph_tex_size[0];
  v->uv[1] = glyph_tex_size[1];

end:
  return err;
}

enum owl_code owl_submit_text_group(struct owl_renderer *renderer,
                                    struct owl_font const *font,
                                    OwlV2 const pos, OwlV3 const color,
                                    char const *text) {
  char const *c;
  OwlV2 cpos; /* move the position to a copy */
  enum owl_code err = OWL_SUCCESS;
  OWL_COPY_V2(pos, cpos);

  /* TODO(samuel): cleanup */
  for (c = text; '\0' != *c; ++c) {
    struct owl_render_group group;

    if (OWL_SUCCESS != (err = owl_fill_char_quad(&renderer->extent, font,
                                                 cpos, color, *c, &group)))
      goto end;

    owl_submit_render_group(renderer, &group);

    cpos[0] +=
        font->glyphs[(int)*c].advance[0] / (float)renderer->extent.width;

    /* FIXME(samuel): not sure if i should substract or add */
    cpos[1] +=
        font->glyphs[(int)*c].advance[1] / (float)renderer->extent.height;
  }

end:
  return err;
}

#include "font.h"

#include "draw_cmd.h"
#include "internal.h"
#include "renderer.h"
#include "vmath.h"

/* clang-format off */
#include <ft2build.h>
#include FT_FREETYPE_H
/* clang-format on */

#define OWL_FIRST_CHAR 32

OWL_INTERNAL void owl_calc_dims_(FT_Face face, int *width, int *height) {
  int i;

  *width = 0;
  *height = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_MAX_GLYPHS; ++i) {
    /* FIXME(samuel): lets close our eyes and pretend this can't fail! */
    FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER);

    *width += (int)face->glyph->bitmap.width;
    *height = OWL_MAX(*height, (int)face->glyph->bitmap.rows);
  }
}

enum owl_code owl_create_font(struct owl_vk_renderer *renderer, int size,
                              char const *path, struct owl_font **font) {
  int i;
  int x;
  owl_byte *data;
  FT_Library ft;
  FT_Face face;
  VkDeviceSize alloc_size;
  struct owl_dyn_buf_ref ref;
  enum owl_code code = OWL_SUCCESS;

  /* TODO(samuel): chances are im only going to be creating one font,
     could do something analogous to owl_vk_texture_manager */
  if (!(*font = OWL_MALLOC(sizeof(**font)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  if (FT_Err_Ok != (FT_Init_FreeType(&ft))) {
    code = OWL_ERROR_BAD_INIT;
    goto end_err_free_font;
  }

  if (FT_Err_Ok != (FT_New_Face(ft, path, 0, &face))) {
    code = OWL_ERROR_BAD_INIT;
    goto end_err_done_ft;
  }

  if (FT_Err_Ok != FT_Set_Char_Size(face, 0, size << 6, 96, 96)) {
    code = OWL_ERROR_UNKNOWN;
    goto end_err_done_face;
  }

  owl_calc_dims_(face, &(*font)->atlas_width, &(*font)->atlas_height);

  alloc_size = (VkDeviceSize)((*font)->atlas_width * (*font)->atlas_height);
  data = owl_dyn_buf_alloc(renderer, alloc_size, &ref);

  if (!data) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_err_done_face;
  }

  x = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_MAX_GLYPHS; ++i) {
    if (FT_Err_Ok != FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER)) {
      code = OWL_ERROR_UNKNOWN;
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

  {
    struct owl_texture_desc desc;
    desc.width = (*font)->atlas_width;
    desc.height = (*font)->atlas_height;
    desc.format = OWL_PIXEL_FORMAT_R8_UNORM;
    desc.mip_mode = OWL_SAMPLER_MIP_MODE_LINEAR;
    desc.min_filter = OWL_SAMPLER_FILTER_LINEAR;
    desc.mag_filter = OWL_SAMPLER_FILTER_LINEAR;
    desc.wrap_u = OWL_SAMPLER_ADDR_MODE_REPEAT;
    desc.wrap_v = OWL_SAMPLER_ADDR_MODE_REPEAT;
    desc.wrap_w = OWL_SAMPLER_ADDR_MODE_REPEAT;

    code = owl_init_vk_texture_from_ref(renderer, &desc, &ref, &(*font)->atlas);

    if (OWL_SUCCESS != code)
      goto end_err_done_face;
  }

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
  return code;
}

void owl_destroy_font(struct owl_vk_renderer *renderer, struct owl_font *font) {
  owl_deinit_vk_texture(renderer, &font->atlas);
  OWL_FREE(font);
}

OWL_INTERNAL enum owl_code owl_fill_char_quad_(int width, int height,
                                               struct owl_font const *font,
                                               owl_v2 const pos,
                                               owl_v3 const color, char c,
                                               struct owl_draw_cmd *group) {
  float uv_off;          /* texture offset in texture coords*/
  owl_v2 screen_pos;     /* position taking into account the bearing in screen
                          coords*/
  owl_v2 glyph_scr_size; /* char size in screen coords */
  owl_v2 glyph_tex_size; /* uv without the offset in texture coords*/

  struct owl_draw_cmd_vertex *v;
  struct owl_glyph const *g = &font->glyphs[(int)c];

  enum owl_code code = OWL_SUCCESS;

  if ((unsigned)c >= OWL_ARRAY_SIZE(font->glyphs)) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  screen_pos[0] = pos[0] + (float)g->bearing[0] / (float)width;
  screen_pos[1] = pos[1] - (float)g->bearing[1] / (float)height;

  glyph_scr_size[0] = (float)g->size[0] / (float)width;
  glyph_scr_size[1] = (float)g->size[1] / (float)height;

  uv_off = (float)g->offset / (float)font->atlas_width;

  glyph_tex_size[0] = (float)g->size[0] / (float)font->atlas_width;
  glyph_tex_size[1] = (float)g->size[1] / (float)font->atlas_height;

  group->type = OWL_DRAW_CMD_TYPE_QUAD;
  group->storage.as_quad.texture = &font->atlas;

  OWL_IDENTITY_M4(group->storage.as_quad.ubo.proj);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.model);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.view);

  v = &group->storage.as_quad.vertices[0];
  v->pos[0] = screen_pos[0];
  v->pos[1] = screen_pos[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off;
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertices[1];
  v->pos[0] = screen_pos[0] + glyph_scr_size[0];
  v->pos[1] = screen_pos[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off + glyph_tex_size[0];
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertices[2];
  v->pos[0] = screen_pos[0];
  v->pos[1] = screen_pos[1] + glyph_scr_size[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off;
  v->uv[1] = glyph_tex_size[1];

  v = &group->storage.as_quad.vertices[3];
  v->pos[0] = screen_pos[0] + glyph_scr_size[0];
  v->pos[1] = screen_pos[1] + glyph_scr_size[1];
  v->pos[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off + glyph_tex_size[0];
  v->uv[1] = glyph_tex_size[1];

end:
  return code;
}

enum owl_code owl_submit_text_group(struct owl_vk_renderer *renderer,
                                    struct owl_font const *font,
                                    owl_v2 const pos, owl_v3 const color,
                                    char const *text) {
  char const *c;
  owl_v2 cpos; /* move the position to a copy */
  enum owl_code code = OWL_SUCCESS;

  OWL_COPY_V2(pos, cpos);

  /* TODO(samuel): cleanup */
  for (c = text; '\0' != *c; ++c) {
    struct owl_draw_cmd group;

    code = owl_fill_char_quad_(renderer->width, renderer->height, font, cpos,
                               color, *c, &group);

    if (OWL_SUCCESS != code)
      goto end;

    owl_submit_draw_cmd(renderer, &group);

    cpos[0] += font->glyphs[(int)*c].advance[0] / (float)renderer->width;

    /* FIXME(samuel): not sure if i should substract or add */
    cpos[1] += font->glyphs[(int)*c].advance[1] / (float)renderer->height;
  }

end:
  return code;
}

#include "font.h"

#include "draw_cmd.h"
#include "internal.h"
#include "renderer.h"
#include "vector_math.h"

/* clang-format off */
#include <ft2build.h>
#include FT_FREETYPE_H
/* clang-format on */

#define OWL_FIRST_CHAR 32


OWL_GLOBAL int ft_library_reference_count = 0;
OWL_GLOBAL FT_Library ft_library;

OWL_INTERNAL void owl_ensure_ft_library_(void) {
  FT_Error err = FT_Err_Ok;

  if (!(ft_library_reference_count++))
    err = FT_Init_FreeType(&ft_library);

  OWL_ASSERT(FT_Err_Ok == err);
}

OWL_INTERNAL void owl_decrement_ft_library_count_(void) {
  OWL_ASSERT(ft_library_reference_count);

  if (!--ft_library_reference_count)
    FT_Done_FreeType(ft_library);
}


OWL_INTERNAL void owl_calc_dims_(FT_Face face, int *width, int *height) {
  int i;

  *width = 0;
  *height = 0;

  for (i = OWL_FIRST_CHAR; i < OWL_FONT_MAX_GLYPHS; ++i) {
    FT_Load_Char(face, (unsigned)i, FT_LOAD_RENDER);

    *width += (int)face->glyph->bitmap.width;
    *height = OWL_MAX(*height, (int)face->glyph->bitmap.rows);
  }
}

OWL_INTERNAL void owl_face_glyph_bitmap_copy_(FT_Face face,  int x_offset,
  int atlas_width, int atlas_height, owl_byte *data) {

  OWL_UNUSED(atlas_height);

  int bx; /* bitmap x position */
  for (bx = 0; bx < (int)face->glyph->bitmap.width; ++bx) {
    int by;                /* bitmap y position, shared by data*/
    int const dx = x_offset + bx; /* atlas x position */

    for (by = 0; by < (int)face->glyph->bitmap.rows; ++by) {
      int const dw = atlas_width;           /* data width */
      int const bw = (int)face->glyph->bitmap.width; /* buf width */

      data[by * dw + dx] = face->glyph->bitmap.buffer[by * bw + bx];
    }
  }
}
                            
OWL_INTERNAL enum owl_code owl_font_init_atlas_(struct owl_vk_renderer *r, 
  struct owl_dynamic_buffer_reference const *ref, struct owl_font *font ) {
  enum owl_code code;
  struct owl_texture_info info;

  info.width = font->atlas_width;
  info.height = font->atlas_height;
  info.format = OWL_PIXEL_FORMAT_R8_UNORM;
  info.mip_mode = OWL_SAMPLER_MIP_MODE_NEAREST;
  info.min_filter = OWL_SAMPLER_FILTER_NEAREST;
  info.mag_filter = OWL_SAMPLER_FILTER_NEAREST;
  info.wrap_u = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  info.wrap_v = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;
  info.wrap_w = OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER;

  code = owl_texture_init_from_ref(r, &info, ref, &font->atlas);

  return code;
}


enum owl_code owl_font_create(struct owl_vk_renderer *r, int size,
                              char const *path, struct owl_font **font) {
  int i;
  int x;
  owl_byte *data;
  FT_Face face;
  VkDeviceSize alloc_size;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  owl_ensure_ft_library_();

  if (!(*font = OWL_MALLOC(sizeof(**font)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  (*font)->size = size;

  if (FT_Err_Ok != (FT_New_Face(ft_library, path, 0, &face))) {
    code = OWL_ERROR_BAD_INIT;
    goto end_err_free_font;
  }

  if (FT_Err_Ok != FT_Set_Char_Size(face, 0, size << 6, 96, 96)) {
    code = OWL_ERROR_UNKNOWN;
    goto end_err_done_face;
  }

  owl_calc_dims_(face, &(*font)->atlas_width, &(*font)->atlas_height);

  alloc_size = (VkDeviceSize)((*font)->atlas_width * (*font)->atlas_height);
  data = owl_renderer_dynamic_buffer_alloc(r, alloc_size, &ref);

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

    owl_face_glyph_bitmap_copy_(face, x, (*font)->atlas_width, (*font)->atlas_height, data);

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

  code = owl_font_init_atlas_(r, &ref, *font);

  if (OWL_SUCCESS != code)
    goto end_err_done_face;

  FT_Done_Face(face);

  goto end;

end_err_done_face:
  FT_Done_Face(face);

end_err_free_font:
  OWL_FREE(*font);

end:
  return code;
}

void owl_font_destroy(struct owl_vk_renderer *r, struct owl_font *font) {
  owl_texture_deinit(r, &font->atlas);
  OWL_FREE(font);
  owl_decrement_ft_library_count_();
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

  OWL_IDENTITY_M4(group->storage.as_quad.ubo.projection);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.model);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.view);

  v = &group->storage.as_quad.vertices[0];
  v->position[0] = screen_pos[0];
  v->position[1] = screen_pos[1];
  v->position[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off;
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertices[1];
  v->position[0] = screen_pos[0] + glyph_scr_size[0];
  v->position[1] = screen_pos[1];
  v->position[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off + glyph_tex_size[0];
  v->uv[1] = 0.0F;

  v = &group->storage.as_quad.vertices[2];
  v->position[0] = screen_pos[0];
  v->position[1] = screen_pos[1] + glyph_scr_size[1];
  v->position[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off;
  v->uv[1] = glyph_tex_size[1];

  v = &group->storage.as_quad.vertices[3];
  v->position[0] = screen_pos[0] + glyph_scr_size[0];
  v->position[1] = screen_pos[1] + glyph_scr_size[1];
  v->position[2] = 0.0F;
  OWL_COPY_V3(color, v->color);
  v->uv[0] = uv_off + glyph_tex_size[0];
  v->uv[1] = glyph_tex_size[1];

end:
  return code;
}

enum owl_code owl_submit_text_group(struct owl_vk_renderer *r,
                                    struct owl_font const *font,
                                    owl_v2 const pos, owl_v3 const color,
                                    char const *text) {
  char const *c;
  owl_v2 cpos; /* move the position to a copy */
  enum owl_code code = OWL_SUCCESS;

  OWL_COPY_V2(pos, cpos);

  for (c = text; '\0' != *c; ++c) {
    struct owl_draw_cmd group;

    code = owl_fill_char_quad_(r->width, r->height, font, cpos,
                               color, *c, &group);

    if (OWL_SUCCESS != code)
      goto end;

    owl_draw_cmd_submit(r, &group);

    cpos[0] += font->glyphs[(int)*c].advance[0] / (float)r->width;
    cpos[1] += font->glyphs[(int)*c].advance[1] / (float)r->height;
  }

end:
  return code;
}

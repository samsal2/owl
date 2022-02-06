
#include "text.h"

#include "draw_cmd.h"
#include "font.h"
#include "internal.h"
#include "renderer.h"
#include "vmath.h"

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

  if ((unsigned)c >= OWL_ARRAY_SIZE(font->glyphs))
    return OWL_ERROR_OUT_OF_BOUNDS;

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

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_text_step_pos_(struct owl_font const *font, char const c,
                                     int framebuffer_width,
                                     int framebuffer_height, owl_v2 pos) {
  pos[0] += font->glyphs[(int)c].advance[0] / (float)framebuffer_width;
  /* FIXME(samuel): not sure if i should substract or add */
  pos[1] += font->glyphs[(int)c].advance[1] / (float)framebuffer_height;
}

enum owl_code owl_text_cmd_submit(struct owl_vk_renderer *r,
                                  struct owl_text_cmd const *cmd) {
  char const *c;
  owl_v2 pos; /* move the position to a copy */
  enum owl_code code = OWL_SUCCESS;

  OWL_COPY_V2(cmd->pos, pos);

  /* TODO(samuel): cleanup */
  for (c = cmd->text; '\0' != *c; ++c) {
    struct owl_draw_cmd quad;

    code = owl_fill_char_quad_(r->width, r->height, cmd->font, pos, cmd->color,
                               *c, &quad);

    if (OWL_SUCCESS != code)
      goto end;

    owl_draw_cmd_submit(r, &quad);
    owl_text_step_pos_(cmd->font, *c, r->width, r->height, pos);
  }

end:
  return code;
}

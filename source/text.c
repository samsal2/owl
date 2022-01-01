
#include "text.h"

#include "font.h"
#include "internal.h"
#include "renderer.h"

OWL_INTERNAL enum owl_code owl_fill_char_quad_(OwlU32 width, OwlU32 height,
                                               struct owl_font const *font,
                                               OwlV2 const pos,
                                               OwlV3 const color, char c,
                                               struct owl_draw_cmd *group) {
  float uv_off;         /* texture offset in texture coords*/
  OwlV2 screen_pos;     /* position taking into account the bearing in screen
                          coords*/
  OwlV2 glyph_scr_size; /* char size in screen coords */
  OwlV2 glyph_tex_size; /* uv without the offset in texture coords*/

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
  group->storage.as_quad.img = &font->atlas;

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

  return OWL_SUCCESS;
}

enum owl_code owl_submit_text_cmd(struct owl_vk_renderer *renderer,
                                  struct owl_text_cmd const *cmd) {
  char const *c;
  OwlV2 pos; /* move the position to a copy */
  enum owl_code err = OWL_SUCCESS;

  OWL_COPY_V2(cmd->pos, pos);

  /* TODO(samuel): cleanup */
  for (c = cmd->text; '\0' != *c; ++c) {
    struct owl_draw_cmd group;

    err = owl_fill_char_quad_(renderer->width, renderer->height, cmd->font, pos,
                              cmd->color, *c, &group);

    if (OWL_SUCCESS != err)
      goto end;

    owl_submit_draw_cmd(renderer, &group);

    pos[0] += cmd->font->glyphs[(int)*c].advance[0] / (float)renderer->width;

    /* FIXME(samuel): not sure if i should substract or add */
    pos[1] += cmd->font->glyphs[(int)*c].advance[1] / (float)renderer->height;
  }

end:
  return err;
}

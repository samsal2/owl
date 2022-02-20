#include "draw_command.h"

#include "font.h"
#include "internal.h"
#include "renderer.h"
#include "skybox.h"
#include "texture.h"
#include "vector_math.h"

OWL_INTERNAL enum owl_code
owl_draw_command_submit_basic_(struct owl_renderer *r,
                               struct owl_draw_command_basic const *basic) {
  enum owl_code code = OWL_SUCCESS;

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(*basic->vertices) * (VkDeviceSize)basic->vertices_count;
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->vertices, size);

    vkCmdBindVertexBuffers(r->frame_command_buffers[r->active], 0, 1,
                           &ref.buffer, &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(*basic->indices) * (VkDeviceSize)basic->indices_count;
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->indices, size);

    vkCmdBindIndexBuffer(r->frame_command_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(basic->ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, &basic->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = basic->texture->set;

    vkCmdBindDescriptorSets(r->frame_command_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.u32_offset);
  }

  vkCmdDrawIndexed(r->frame_command_buffers[r->active],
                   (owl_u32)basic->indices_count, 1, 0, 0, 0);

  return code;
}

OWL_INTERNAL enum owl_code
owl_draw_command_submit_quad_(struct owl_renderer *r,
                              struct owl_draw_command_quad const *quad) {
  enum owl_code code = OWL_SUCCESS;

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(quad->vertices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, quad->vertices, size);

    vkCmdBindVertexBuffers(r->frame_command_buffers[r->active], 0, 1,
                           &ref.buffer, &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;
    owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(r->frame_command_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(quad->ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, &quad->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = quad->texture->set;

    vkCmdBindDescriptorSets(r->frame_command_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.u32_offset);
  }

  vkCmdDrawIndexed(r->frame_command_buffers[r->active], 6, 1, 0, 0, 0);

  return code;
}

OWL_INTERNAL enum owl_code
owl_draw_command_submit_skybox_(struct owl_renderer *r,
                                struct owl_draw_command_skybox const *skybox) {
  enum owl_code code = OWL_SUCCESS;

  OWL_LOCAL_PERSIST owl_v3 const vertices[] = {
#if 1
    {-1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},

    {-1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 1.0f},
#endif

#if 1
    {1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},

    {1.0f, 1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
#endif

#if 1
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f}
#endif
  };

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(vertices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, vertices, size);

    vkCmdBindVertexBuffers(r->frame_command_buffers[r->active], 0, 1,
                           &ref.buffer, &ref.offset);
  }

#if 0
  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;
    owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(r->frame_command_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }
#endif

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;
    struct owl_skybox_ubo ubo;

    size = sizeof(struct owl_skybox_ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

#if 1
    OWL_M4_COPY(skybox->ubo.projection, ubo.projection);
#else
    OWL_M4_IDENTITY(ubo.projection);
#endif

#if 1
    OWL_M4_IDENTITY(ubo.view);
    OWL_M3_COPY(skybox->ubo.view, ubo.view);
#else
    OWL_M4_IDENTITY(ubo.view);
#endif

    OWL_MEMCPY(data, &ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = skybox->skybox->set;

    vkCmdBindDescriptorSets(r->frame_command_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.u32_offset);
  }

#if 0
  vkCmdDrawIndexed(r->frame_command_buffers[r->active], 6, 1, 0, 0, 0);
#else
  vkCmdDraw(r->frame_command_buffers[r->active], OWL_ARRAY_SIZE(vertices), 1, 0,
            0);
#endif

  return code;
}

OWL_INTERNAL enum owl_code owl_draw_command_fill_char_quad_(
    struct owl_renderer const *r, struct owl_draw_command_text const *text,
    char c, owl_v2 offset, struct owl_draw_command_quad *quad) {

  float uv_offset;
  owl_v2 uv_bearing;
  owl_v2 current_position;
  owl_v2 screen_position;
  owl_v2 glyph_uv_size;
  owl_v2 glyph_screen_size;
  struct owl_glyph const *glyph;
  enum owl_code code = OWL_SUCCESS;

  if ((int)c >= (int)OWL_ARRAY_SIZE(text->font->glyphs)) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  quad->texture = &text->font->atlas;

  OWL_M4_IDENTITY(quad->ubo.projection);
  OWL_M4_IDENTITY(quad->ubo.view);
  OWL_M4_IDENTITY(quad->ubo.model);

  glyph = &text->font->glyphs[(int)c];

  OWL_V2_ADD(text->position, offset, current_position);

  /* TODO(samuel): save the bearing as a normalized value */
  uv_bearing[0] = (float)glyph->bearing[0] / (float)r->width;
  uv_bearing[1] = -(float)glyph->bearing[1] / (float)r->height;

  OWL_V2_ADD(current_position, uv_bearing, screen_position);

  /* TODO(samuel): save the size as a normalized value */
  glyph_screen_size[0] = (float)glyph->size[0] / (float)r->width;
  glyph_screen_size[1] = (float)glyph->size[1] / (float)r->height;

  /* TODO(samuel): save the size as a normalized value */
  glyph_uv_size[0] = (float)glyph->size[0] / (float)text->font->atlas_width;
  glyph_uv_size[1] = (float)glyph->size[1] / (float)text->font->atlas_height;

  /* TODO(samuel): save the offset as a normalized value */
  uv_offset = (float)glyph->offset / (float)text->font->atlas_width;

  quad->vertices[0].position[0] = screen_position[0];
  quad->vertices[0].position[1] = screen_position[1];
  quad->vertices[0].position[2] = 0.0F;
  quad->vertices[0].color[0] = text->color[0];
  quad->vertices[0].color[1] = text->color[1];
  quad->vertices[0].color[2] = text->color[2];
  quad->vertices[0].uv[0] = uv_offset;
  quad->vertices[0].uv[1] = 0.0F;

  quad->vertices[1].position[0] = screen_position[0] + glyph_screen_size[0];
  quad->vertices[1].position[1] = screen_position[1];
  quad->vertices[1].position[2] = 0.0F;
  quad->vertices[1].color[0] = text->color[0];
  quad->vertices[1].color[1] = text->color[1];
  quad->vertices[1].color[2] = text->color[2];
  quad->vertices[1].uv[0] = uv_offset + glyph_uv_size[0];
  quad->vertices[1].uv[1] = 0.0F;

  quad->vertices[2].position[0] = screen_position[0];
  quad->vertices[2].position[1] = screen_position[1] + glyph_screen_size[1];
  quad->vertices[2].position[2] = 0.0F;
  quad->vertices[2].color[0] = text->color[0];
  quad->vertices[2].color[1] = text->color[1];
  quad->vertices[2].color[2] = text->color[2];
  quad->vertices[2].uv[0] = uv_offset;
  quad->vertices[2].uv[1] = glyph_uv_size[1];

  quad->vertices[3].position[0] = screen_position[0] + glyph_screen_size[0];
  quad->vertices[3].position[1] = screen_position[1] + glyph_screen_size[1];
  quad->vertices[3].position[2] = 0.0F;
  quad->vertices[3].color[0] = text->color[0];
  quad->vertices[3].color[1] = text->color[1];
  quad->vertices[3].color[2] = text->color[2];
  quad->vertices[3].uv[0] = uv_offset + glyph_uv_size[0];
  quad->vertices[3].uv[1] = glyph_uv_size[1];

end:
  return code;
}

OWL_INTERNAL void owl_font_step_offset_(struct owl_renderer const *r,
                                        struct owl_font const *font,
                                        char const c, owl_v2 offset) {
  offset[0] += font->glyphs[(int)c].advance[0] / (float)r->width;
  /* FIXME(samuel): not sure if i should substract or add */
  offset[1] += font->glyphs[(int)c].advance[1] / (float)r->height;
}

OWL_INTERNAL enum owl_code
owl_draw_command_submit_text_(struct owl_renderer *r,
                              struct owl_draw_command_text const *text) {
  char const *c;
  owl_v2 offset;
  enum owl_code code = OWL_SUCCESS;

  OWL_V2_ZERO(offset);

  for (c = text->text; '\0' != *c; ++c) {
    struct owl_draw_command_quad quad;

    code = owl_draw_command_fill_char_quad_(r, text, *c, offset, &quad);

    if (OWL_SUCCESS != code)
      goto end;

    owl_draw_command_submit_quad_(r, &quad);
    owl_font_step_offset_(r, text->font, *c, offset);
  }

end:
  return code;
}

enum owl_code owl_draw_command_submit(struct owl_renderer *r,
                                      struct owl_draw_command const *command) {
  switch (command->type) {
  case OWL_DRAW_COMMAND_TYPE_BASIC:
    return owl_draw_command_submit_basic_(r, &command->storage.as_basic);

  case OWL_DRAW_COMMAND_TYPE_QUAD:
    return owl_draw_command_submit_quad_(r, &command->storage.as_quad);

  case OWL_DRAW_COMMAND_TYPE_SKYBOX:
    return owl_draw_command_submit_skybox_(r, &command->storage.as_skybox);

  case OWL_DRAW_COMMAND_TYPE_TEXT:
    return owl_draw_command_submit_text_(r, &command->storage.as_text);
  }
}

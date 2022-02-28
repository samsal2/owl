#include "draw.h"

#include "camera.h"
#include "font.h"
#include "internal.h"
#include "renderer.h"
#include "scene.h"
#include "skybox.h"
#include "vector_math.h"

OWL_INTERNAL enum owl_code
owl_renderer_submit_vertices_(struct owl_renderer *r, owl_u32 count,
                              struct owl_draw_vertex const *vertices) {
  owl_byte *data;
  VkDeviceSize size;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  size = sizeof(*vertices) * (VkDeviceSize)count;
  data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

  OWL_MEMCPY(data, vertices, size);

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &ref.buffer,
                         &ref.offset);

  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_submit_indices_(struct owl_renderer *r, owl_u32 count,
                             owl_u32 const *indices) {
  owl_byte *data;
  VkDeviceSize size;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  size = sizeof(*indices) * (VkDeviceSize)count;
  data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

  OWL_MEMCPY(data, indices, size);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, ref.buffer, ref.offset,
                       VK_INDEX_TYPE_UINT32);

  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_submit_uniforms_(struct owl_renderer *r, owl_m4 const model,
                              struct owl_camera const *cam,
                              struct owl_texture const *texture) {
  owl_byte *data;
  VkDescriptorSet sets[2];
  struct owl_draw_uniform uniform;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  OWL_M4_COPY(cam->projection, uniform.projection);
  OWL_M4_COPY(cam->view, uniform.view);
  OWL_M4_COPY(model, uniform.model);

  data = owl_renderer_dynamic_buffer_alloc(r, sizeof(uniform), &ref);
  OWL_MEMCPY(data, &uniform, sizeof(uniform));

  sets[0] = ref.pvm_set;
  sets[1] = texture->set;

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &ref.offset32);

  return code;
}

enum owl_code
owl_submit_draw_basic_command(struct owl_renderer *r,
                              struct owl_camera const *cam,
                              struct owl_draw_basic_command const *command) {
  enum owl_code code = OWL_SUCCESS;

  code = owl_renderer_submit_vertices_(r, command->vertices_count,
                                       command->vertices);

  if (OWL_SUCCESS != code)
    goto end;

  code =
      owl_renderer_submit_indices_(r, command->indices_count, command->indices);

  if (OWL_SUCCESS != code)
    goto end;

  code =
      owl_renderer_submit_uniforms_(r, command->model, cam, command->texture);

  if (OWL_SUCCESS != code)
    goto end;

  vkCmdDrawIndexed(r->active_frame_command_buffer, command->indices_count, 1, 0,
                   0, 0);

end:
  return code;
}

enum owl_code
owl_submit_draw_quad_command(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_quad_command const *command) {
  enum owl_code code = OWL_SUCCESS;
  owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  code = owl_renderer_submit_vertices_(r, 4, command->vertices);

  if (OWL_SUCCESS != code)
    goto end;

  code = owl_renderer_submit_indices_(r, 6, indices);

  if (OWL_SUCCESS != code)
    goto end;

  code =
      owl_renderer_submit_uniforms_(r, command->model, cam, command->texture);

  if (OWL_SUCCESS != code)
    goto end;

  vkCmdDrawIndexed(r->active_frame_command_buffer, 6, 1, 0, 0, 0);

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_draw_text_command_fill_char_quad_(
    struct owl_draw_text_command const *text, int framebuffer_width,
    int framebuffer_height, owl_v3 const offset, char c,
    struct owl_draw_quad_command *quad) {
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

  OWL_M4_IDENTITY(quad->model);
  owl_m4_translate(text->position, quad->model);

  glyph = &text->font->glyphs[(int)c];

  OWL_V2_ADD(text->position, offset, current_position);

  /* TODO(samuel): save the bearing as a normalized value */
  uv_bearing[0] = (float)glyph->bearing[0] / (float)framebuffer_width;
  uv_bearing[1] = -(float)glyph->bearing[1] / (float)framebuffer_height;

  OWL_V2_ADD(current_position, uv_bearing, screen_position);

  /* TODO(samuel): save the size as a normalized value */
  glyph_screen_size[0] = (float)glyph->size[0] / (float)framebuffer_width;
  glyph_screen_size[1] = (float)glyph->size[1] / (float)framebuffer_height;

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

OWL_INTERNAL void owl_font_step_offset_(int framebuffer_width,
                                        int framebuffer_height,
                                        struct owl_font const *font,
                                        char const c, owl_v2 offset) {
  offset[0] += font->glyphs[(int)c].advance[0] / (float)framebuffer_width;
  /* FIXME(samuel): not sure if i should substract or add */
  offset[1] += font->glyphs[(int)c].advance[1] / (float)framebuffer_height;
}

enum owl_code
owl_submit_draw_text_command(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_text_command const *command) {
  char const *c;
  owl_v2 offset;
  enum owl_code code = OWL_SUCCESS;

  OWL_V2_ZERO(offset);

  for (c = command->text; '\0' != *c; ++c) {
    struct owl_draw_quad_command quad;

    code = owl_draw_text_command_fill_char_quad_(command, r->width, r->height,
                                                 offset, *c, &quad);

    if (OWL_SUCCESS != code)
      goto end;

    owl_submit_draw_quad_command(r, cam, &quad);
    owl_font_step_offset_(r->width, r->height, command->font, *c, offset);
  }

end:
  return code;
}

OWL_GLOBAL owl_v3 const g_skybox_vertices[] = {
    {-1.0f, 1.0f, -1.0f},  {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},  {1.0f, 1.0f, -1.0f},   {-1.0f, 1.0f, -1.0f},

    {-1.0f, -1.0f, 1.0f},  {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},

    {-1.0f, 1.0f, -1.0f},  {-1.0f, 1.0f, 1.0f},   {-1.0f, -1.0f, 1.0f},

    {1.0f, -1.0f, -1.0f},  {1.0f, -1.0f, 1.0f},   {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},    {1.0f, 1.0f, -1.0f},   {1.0f, -1.0f, -1.0f},

    {-1.0f, -1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},  {1.0f, 1.0f, -1.0f},

    {1.0f, 1.0f, -1.0f},   {1.0f, -1.0f, -1.0f},  {-1.0f, -1.0f, -1.0f},

    {-1.0f, 1.0f, -1.0f},  {1.0f, 1.0f, -1.0f},   {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f},   {-1.0f, 1.0f, -1.0f},

    {-1.0f, -1.0f, -1.0f}, {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},  {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f}};

enum owl_code
owl_submit_draw_skybox_command(struct owl_renderer *r,
                               struct owl_camera const *cam,
                               struct owl_draw_skybox_command const *command) {
  owl_byte *data;
  VkDescriptorSet sets[2];
  struct owl_draw_uniform uniform;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  data = owl_renderer_dynamic_buffer_alloc(r, sizeof(g_skybox_vertices), &ref);
  OWL_MEMCPY(data, g_skybox_vertices, sizeof(g_skybox_vertices));

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &ref.buffer,
                         &ref.offset);

  OWL_M4_COPY(cam->projection, uniform.projection);
  OWL_M4_IDENTITY(uniform.view);
  OWL_M3_COPY(cam->view, uniform.view);
  OWL_M4_IDENTITY(uniform.model);

  data = owl_renderer_dynamic_buffer_alloc(r, sizeof(uniform), &ref);
  OWL_MEMCPY(data, &uniform, sizeof(uniform));

  sets[0] = ref.pvm_set;
  sets[1] = command->skybox->set;

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &ref.offset32);

  vkCmdDraw(r->active_frame_command_buffer, OWL_ARRAY_SIZE(g_skybox_vertices),
            1, 0, 0);

  return code;
}

OWL_INTERNAL
enum owl_code
owl_scene_submit_node_(struct owl_renderer *r, struct owl_camera *cam,
                       struct owl_draw_scene_command const *command,
                       owl_scene_node node) {

  int i;
  struct owl_scene_push_constant push_constant;
  enum owl_code code = OWL_SUCCESS;
  owl_scene_node current = node;
  struct owl_scene_node_data const *data = &command->scene->nodes[node];

  for (i = 0; i < data->children_count; ++i) {
    code = owl_scene_submit_node_(r, cam, command, data->children[node]);

    if (OWL_SUCCESS != code)
      goto end;
  }

  if (!data->mesh.primitives_count)
    goto end;

  OWL_M4_COPY(data->matrix, push_constant.model);

  while (OWL_SCENE_NODE_NO_PARENT != current) {
    struct owl_scene_node_data const *current_data =
        &command->scene->nodes[current];
    owl_m4_mul_rotation(push_constant.model, current_data->matrix,
                        push_constant.model);
    current = current_data->parent;
  }

  // OWL_V3_SET(0.0F, 0.0F, -1.0F, position);
  // owl_m4_translate(position, push_constant.model);

  vkCmdPushConstants(r->active_frame_command_buffer, r->active_pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant),
                     &push_constant);

  for (i = 0; i < data->mesh.primitives_count; ++i) {
    owl_byte *upload;
    VkDescriptorSet sets[3];
    struct owl_scene_uniform uniform;
    struct owl_dynamic_buffer_reference ref;
    struct owl_scene_primitive const *primitive = &data->mesh.primitives[i];
    struct owl_scene_material const *material =
        &command->scene->materials[primitive->material];
    owl_scene_texture base =
        command->scene->textures[material->base_color_texture_index];
    owl_scene_texture normal =
        command->scene->textures[material->base_color_texture_index];
    struct owl_texture const *base_image = &command->scene->images[base];
    struct owl_texture const *normal_image = &command->scene->images[normal];

    OWL_M4_COPY(cam->projection, uniform.projection);
    OWL_M4_COPY(cam->view, uniform.view);
    OWL_V4_ZERO(uniform.light);
    OWL_V3_COPY(command->light, uniform.light);

    OWL_V4_ZERO(uniform.eye);
    OWL_V3_COPY(cam->eye, uniform.eye);

    uniform.enable_alpha_mask = (int)material->alpha_mode;
    uniform.alpha_cutoff = material->alpha_cutoff;

    upload = owl_renderer_dynamic_buffer_alloc(r, sizeof(uniform), &ref);
    OWL_MEMCPY(upload, &uniform, sizeof(uniform));

    sets[0] = ref.scene_set;
    sets[1] = base_image->set;
    sets[2] = normal_image->set;

    vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                            sets, 1, &ref.offset32);

    vkCmdDrawIndexed(r->active_frame_command_buffer, primitive->count, 1,
                     primitive->first, 0, 0);
  }

end:
  return code;
}

enum owl_code
owl_submit_draw_scene_command(struct owl_renderer *r, struct owl_camera *cam,
                              struct owl_draw_scene_command const *command) {
  int i;
  VkDeviceSize offset = 0;
  enum owl_code code = OWL_SUCCESS;

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1,
                         &command->scene->vertices_buffer, &offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer,
                       command->scene->indices_buffer, 0, VK_INDEX_TYPE_UINT32);

  for (i = 0; i < command->scene->roots_count; ++i) {
    code = owl_scene_submit_node_(r, cam, command, command->scene->roots[i]);

    if (OWL_SUCCESS != code)
      goto end;
  }

end:
  return code;
}

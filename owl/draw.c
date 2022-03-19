#include "draw.h"

#include "camera.h"
#include "font.h"
#include "internal.h"
#include "model.h"
#include "renderer.h"
#include "vector_math.h"

enum owl_code
owl_submit_draw_basic_command(struct owl_renderer *r,
                              struct owl_camera const *c,
                              struct owl_draw_basic_command const *command) {
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_draw_uniform uniform;
  struct owl_dynamic_heap_reference vdhr;
  struct owl_dynamic_heap_reference idhr;
  struct owl_dynamic_heap_reference udhr;
  enum owl_code code = OWL_SUCCESS;

  size = (owl_u64)command->vertices_count * sizeof(*command->vertices);
  code = owl_renderer_dynamic_heap_submit(r, size, command->vertices, &vdhr);

  if (OWL_SUCCESS != code)
    goto end;

  size = (owl_u64)command->indices_count * sizeof(*command->indices);
  code = owl_renderer_dynamic_heap_submit(r, size, command->indices, &idhr);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_M4_COPY(c->projection, uniform.projection);
  OWL_M4_COPY(c->view, uniform.view);
  OWL_M4_COPY(command->model, uniform.model);

  size = sizeof(uniform);
  code = owl_renderer_dynamic_heap_submit(r, size, &uniform, &udhr);

  if (OWL_SUCCESS != code)
    goto end;

  sets[0] = udhr.pvm_set;
  sets[1] = r->image_manager_sets[command->image.slot];

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &vdhr.buffer,
                         &vdhr.offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, idhr.buffer, idhr.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &udhr.offset32);

  vkCmdDrawIndexed(r->active_frame_command_buffer, command->indices_count, 1, 0,
                   0, 0);

end:
  return code;
}

enum owl_code
owl_submit_draw_quad_command(struct owl_renderer *r, struct owl_camera const *c,
                             struct owl_draw_quad_command const *command) {
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_draw_uniform uniform;
  struct owl_dynamic_heap_reference vdhr;
  struct owl_dynamic_heap_reference idhr;
  struct owl_dynamic_heap_reference udhr;
  enum owl_code code = OWL_SUCCESS;
  owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  size = OWL_ARRAY_SIZE(command->vertices) * sizeof(command->vertices[0]);
  code = owl_renderer_dynamic_heap_submit(r, size, command->vertices, &vdhr);

  if (OWL_SUCCESS != code)
    goto end;

  size = OWL_ARRAY_SIZE(indices) * sizeof(indices[0]);
  code = owl_renderer_dynamic_heap_submit(r, size, indices, &idhr);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_M4_COPY(c->projection, uniform.projection);
  OWL_M4_COPY(c->view, uniform.view);
  OWL_M4_COPY(command->model, uniform.model);

  size = sizeof(uniform);
  code = owl_renderer_dynamic_heap_submit(r, size, &uniform, &udhr);

  if (OWL_SUCCESS != code)
    goto end;

  sets[0] = udhr.pvm_set;
  sets[1] = r->image_manager_sets[command->image.slot];

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &vdhr.buffer,
                         &vdhr.offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, idhr.buffer, idhr.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &udhr.offset32);

  vkCmdDrawIndexed(r->active_frame_command_buffer, OWL_ARRAY_SIZE(indices), 1,
                   0, 0, 0);
end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_draw_text_command_fill_char_quad_(struct owl_draw_text_command const *text,
                                      owl_v3 const offset, char c,
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

  quad->image.slot = text->font->atlas.slot;

  OWL_M4_IDENTITY(quad->model);
  owl_m4_translate(text->position, quad->model);

  glyph = &text->font->glyphs[(int)c];

  OWL_V2_ADD(text->position, offset, current_position);

  /* TODO(samuel): save the bearing as a normalized value */
  uv_bearing[0] =
      (float)glyph->bearing[0] / (float)(text->font->size) * text->scale;
  uv_bearing[1] =
      -(float)glyph->bearing[1] / (float)(text->font->size) * text->scale;

  OWL_V2_ADD(current_position, uv_bearing, screen_position);

  /* TODO(samuel): save the size as a normalized value */
#if 1
  glyph_screen_size[0] =
      (float)glyph->size[0] / (float)(text->font->size) * text->scale;
  glyph_screen_size[1] =
      (float)glyph->size[1] / (float)(text->font->size) * text->scale;
#else
  glyph_screen_size[0] = (float)glyph->size[0] / (float)1200;
  glyph_screen_size[1] = (float)glyph->size[1] / (float)1200;
#endif

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

OWL_INTERNAL void
owl_draw_text_command_step_offset_(struct owl_draw_text_command const *command,
                                   char const c, owl_v2 offset) {
  offset[0] += command->font->glyphs[(int)c].advance[0] /
               (float)(command->font->size) * command->scale;
  /* FIXME(samuel): not sure if i should substract or add */
  offset[1] -= command->font->glyphs[(int)c].advance[1] /
               (float)(command->font->size) * command->scale;
}

enum owl_code
owl_submit_draw_text_command(struct owl_renderer *r, struct owl_camera const *c,
                             struct owl_draw_text_command const *command) {
  char const *l;
  owl_v2 offset;
  enum owl_code code = OWL_SUCCESS;

  OWL_V2_ZERO(offset);

  for (l = command->text; '\0' != *l; ++l) {
    struct owl_draw_quad_command quad;

    code = owl_draw_text_command_fill_char_quad_(command, offset, *l, &quad);

    if (OWL_SUCCESS != code)
      goto end;

    owl_submit_draw_quad_command(r, c, &quad);
    owl_draw_text_command_step_offset_(command, *l, offset);
  }

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_submit_node_(struct owl_renderer *r, struct owl_camera *c,
                       struct owl_draw_model_command const *command,
                       struct owl_model_node const *node) {

  int i;
  struct owl_model_node parent;
  struct owl_model const *model;
  struct owl_model_node_data const *node_data;
  struct owl_model_mesh_data const *mesh_data;
  struct owl_model_push_constant push_constant;
  enum owl_code code = OWL_SUCCESS;

  model = command->model;
  node_data = &model->nodes[node->slot];

  for (i = 0; i < node_data->children_count; ++i) {
    code = owl_model_submit_node_(r, c, command, &node_data->children[i]);

    if (OWL_SUCCESS != code)
      goto end;
  }

  if (OWL_MODEL_NODE_NO_MESH_SLOT == node_data->mesh.slot)
    goto end;

  mesh_data = &model->meshes[node_data->mesh.slot];

  if (!mesh_data->primitives_count)
    goto end;

  OWL_M4_COPY(model->nodes[node->slot].matrix, push_constant.model);

  /* HACK(samuel): invert the y coordinate */
  push_constant.model[2][2] *= -1.0F;

  /* HACK(samuel): manually change the offset */
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate(offset, push_constant.model);
  }

  for (parent.slot = model->nodes[node->slot].parent.slot;
       OWL_MODEL_NODE_NO_PARENT_SLOT != parent.slot;
       parent.slot = model->nodes[parent.slot].parent.slot)
    owl_m4_multiply(model->nodes[parent.slot].matrix, push_constant.model,
                    push_constant.model);

  vkCmdPushConstants(r->active_frame_command_buffer, r->active_pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constant),
                     &push_constant);

  vkCmdBindDescriptorSets(
      r->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      r->active_pipeline_layout, 3, 1,
      &model->skins[node_data->skin.slot].ssbo_sets[r->frame], 0, 0);

  for (i = 0; i < mesh_data->primitives_count; ++i) {
    VkDescriptorSet sets[3];
    struct owl_model_primitive primitive;
    struct owl_model_uniform uniform;
    struct owl_dynamic_heap_reference dhr;
    struct owl_model_material material;
    struct owl_model_texture base_color_texture;
    struct owl_model_texture normal_texture;
    struct owl_model_image base_color_image;
    struct owl_model_image normal_image;
    struct owl_model_primitive_data const *primitive_data;
    struct owl_model_material_data const *material_data;
    struct owl_model_texture_data const *base_color_texture_data;
    struct owl_model_texture_data const *normal_texture_data;
    struct owl_model_image_data const *base_color_image_data;
    struct owl_model_image_data const *normal_image_data;

    primitive.slot = mesh_data->primitives[i].slot;
    primitive_data = &model->primitives[primitive.slot];

    if (!primitive_data->count)
      continue;

    material.slot = primitive_data->material.slot;
    material_data = &model->materials[material.slot];

    base_color_texture.slot = material_data->base_color_texture.slot;
    base_color_texture_data = &model->textures[base_color_texture.slot];

    normal_texture.slot = material_data->normal_texture.slot;
    normal_texture_data = &model->textures[normal_texture.slot];

    base_color_image.slot = base_color_texture_data->image.slot;
    base_color_image_data = &model->images[base_color_image.slot];

    normal_image.slot = normal_texture_data->image.slot;
    normal_image_data = &model->images[normal_image.slot];

    OWL_M4_COPY(c->projection, uniform.projection);
    OWL_M4_COPY(c->view, uniform.view);
    OWL_V4_ZERO(uniform.light);
    OWL_V3_COPY(command->light, uniform.light);

    code = owl_renderer_dynamic_heap_submit(r, sizeof(uniform), &uniform, &dhr);

    if (OWL_SUCCESS != code)
      goto end;

    sets[0] = dhr.pvl_set;
    sets[1] = r->image_manager_sets[base_color_image_data->image.slot];
    sets[2] = r->image_manager_sets[normal_image_data->image.slot];

    vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                            sets, 1, &dhr.offset32);

    vkCmdDrawIndexed(r->active_frame_command_buffer, primitive_data->count, 1,
                     primitive_data->first, 0, 0);
  }

end:
  return code;
}

enum owl_code
owl_submit_draw_model_command(struct owl_renderer *r, struct owl_camera *c,
                              struct owl_draw_model_command const *command) {
  int i;
  VkDeviceSize offset = 0;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model const *model = command->model;

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1,
                         &model->vertices_buffer, &offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, model->indices_buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  for (i = 0; i < command->model->roots_count; ++i) {
    code = owl_model_submit_node_(r, c, command, &model->roots[i]);

    if (OWL_SUCCESS != code)
      goto end;
  }

end:
  return code;
}

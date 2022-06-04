#include "owl-vk-draw.h"

#include "owl-internal.h"
#include "owl-model.h"
#include "owl-vector-math.h"
#include "owl-vk-font.h"
#include "owl-vk-frame.h"
#include "owl-vk-renderer.h"
#include "owl-vk-texture.h"
#include "owl-vk-types.h"

owl_public enum owl_code
owl_vk_draw_quad(struct owl_vk_renderer *vk, struct owl_quad const *quad,
                 owl_m4 const matrix)
{
  owl_byte *data;
  VkDescriptorSet sets[2];
  VkCommandBuffer command_buffer;
  struct owl_pvm_ubo ubo;
  struct owl_pcu_vertex vertices[4];
  struct owl_vk_frame_allocation valloc;
  struct owl_vk_frame_allocation ialloc;
  struct owl_vk_frame_allocation ualloc;

  owl_local_persist owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  command_buffer = vk->frame_command_buffers[vk->frame];

  vertices[0].position[0] = quad->position0[0];
  vertices[0].position[1] = quad->position0[1];
  vertices[0].position[2] = 0.0F;
  vertices[0].color[0] = quad->color[0];
  vertices[0].color[1] = quad->color[1];
  vertices[0].color[2] = quad->color[2];
  vertices[0].uv[0] = quad->uv0[0];
  vertices[0].uv[1] = quad->uv0[1];

  vertices[1].position[0] = quad->position1[0];
  vertices[1].position[1] = quad->position0[1];
  vertices[1].position[2] = 0.0F;
  vertices[1].color[0] = quad->color[0];
  vertices[1].color[1] = quad->color[1];
  vertices[1].color[2] = quad->color[2];
  vertices[1].uv[0] = quad->uv1[0];
  vertices[1].uv[1] = quad->uv0[1];

  vertices[2].position[0] = quad->position0[0];
  vertices[2].position[1] = quad->position1[1];
  vertices[2].position[2] = 0.0F;
  vertices[2].color[0] = quad->color[0];
  vertices[2].color[1] = quad->color[1];
  vertices[2].color[2] = quad->color[2];
  vertices[2].uv[0] = quad->uv0[0];
  vertices[2].uv[1] = quad->uv1[1];

  vertices[3].position[0] = quad->position1[0];
  vertices[3].position[1] = quad->position1[1];
  vertices[3].position[2] = 0.0F;
  vertices[3].color[0] = quad->color[0];
  vertices[3].color[1] = quad->color[1];
  vertices[3].color[2] = quad->color[2];
  vertices[3].uv[0] = quad->uv1[0];
  vertices[3].uv[1] = quad->uv1[1];

  owl_m4_copy(vk->projection, ubo.projection);
  owl_m4_copy(vk->view, ubo.view);
  owl_m4_copy(matrix, ubo.model);

  data = owl_vk_frame_alloc(vk, sizeof(vertices), &valloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, vertices, sizeof(vertices));

  data = owl_vk_frame_alloc(vk, sizeof(indices), &ialloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, indices, sizeof(indices));

  data = owl_vk_frame_alloc(vk, sizeof(ubo), &ualloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, &ubo, sizeof(ubo));

  sets[0] = ualloc.pvm_descriptor_set;
  sets[1] = quad->texture->descriptor_set;

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &valloc.buffer, &valloc.offset);

  vkCmdBindIndexBuffer(command_buffer, ialloc.buffer, ialloc.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vk->pipeline_layouts[vk->pipeline], 0,
                          owl_array_size(sets), sets, 1, &ualloc.offset32);

  vkCmdDrawIndexed(command_buffer, owl_array_size(indices), 1, 0, 0, 0);

  return OWL_OK;
}

owl_private enum owl_code
owl_vk_draw_glyph(struct owl_vk_renderer *vk, struct owl_vk_glyph *glyph,
                  owl_v3 const color)
{
  owl_m4 matrix;
  owl_v3 scale;
  struct owl_quad quad;

  scale[0] = 2.0F / (float)vk->height;
  scale[1] = 2.0F / (float)vk->height;
  scale[2] = 2.0F / (float)vk->height;

  owl_m4_identity(matrix);
  owl_m4_scale_v3(matrix, scale, matrix);

  quad.texture = &vk->font_atlas;

  quad.position0[0] = glyph->positions[0][0];
  quad.position0[1] = glyph->positions[0][1];

  quad.position1[0] = glyph->positions[3][0];
  quad.position1[1] = glyph->positions[3][1];

  quad.color[0] = color[0];
  quad.color[1] = color[1];
  quad.color[2] = color[2];

  quad.uv0[0] = glyph->uvs[0][0];
  quad.uv0[1] = glyph->uvs[0][1];

  quad.uv1[0] = glyph->uvs[3][0];
  quad.uv1[1] = glyph->uvs[3][1];

  return owl_vk_draw_quad(vk, &quad, matrix);
}

owl_public enum owl_code
owl_vk_draw_text(struct owl_vk_renderer *vk, char const *text,
                 owl_v3 const position, owl_v3 const color)
{
  char const *c;
  owl_v2 offset;

  enum owl_code code;

  owl_vk_bind_pipeline(vk, OWL_VK_PIPELINE_TEXT);

  offset[0] = position[0] * (float)vk->width;
  offset[1] = position[1] * (float)vk->height;

  for (c = &text[0]; '\0' != *c; ++c) {
    struct owl_vk_glyph glyph;

    code = owl_vk_font_fill_glyph(vk, *c, offset, &glyph);
    if (code)
      return code;

    code = owl_vk_draw_glyph(vk, &glyph, color);
    if (code)
      return code;
  }

  return OWL_OK;
}

owl_private enum owl_code
owl_vk_draw_model_node(struct owl_vk_renderer *vk, owl_model_node_id id,
                       struct owl_model const *model, owl_m4 const matrix)
{
  owl_i32 i;
  owl_byte *data;
  owl_model_node_id parent;
  struct owl_model_node const *node;
  struct owl_model_mesh const *mesh;
  struct owl_model_skin const *skin;
  struct owl_model_skin_ssbo *ssbo;
  struct owl_model_ubo1 ubo1;
  struct owl_model_ubo2 ubo2;
  struct owl_vk_frame_allocation u1alloc;
  struct owl_vk_frame_allocation u2alloc;

  enum owl_code code;

  node = &model->nodes[id];

  for (i = 0; i < node->child_count; ++i) {
    code = owl_vk_draw_model_node(vk, node->children[i], model, matrix);
    if (OWL_OK != code)
      return code;
  }

  if (OWL_MODEL_MESH_NONE == node->mesh)
    return OWL_OK;

  mesh = &model->meshes[node->mesh];
  skin = &model->skins[node->skin];
  ssbo = skin->ssbos[vk->frame];

  owl_m4_copy(node->matrix, ssbo->matrix);

  for (parent = node->parent; OWL_MODEL_NODE_NONE != parent;
       parent = model->nodes[parent].parent)
    owl_m4_multiply(model->nodes[parent].matrix, ssbo->matrix, ssbo->matrix);

  owl_m4_copy(vk->projection, ubo1.projection);
  owl_m4_copy(vk->view, ubo1.view);
  owl_m4_copy(matrix, ubo1.model);
  owl_v4_zero(ubo1.light);
  owl_v4_zero(ubo2.light_direction);

  data = owl_vk_frame_alloc(vk, sizeof(ubo1), &u1alloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, &ubo1, sizeof(ubo1));

  vkCmdBindDescriptorSets(
      vk->frame_command_buffers[vk->frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk->pipeline_layouts[vk->pipeline], 0, 1, &u1alloc.model1_descriptor_set,
      1, &u1alloc.offset32);

  data = owl_vk_frame_alloc(vk, sizeof(ubo2), &u2alloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, &ubo2, sizeof(ubo2));

  vkCmdBindDescriptorSets(
      vk->frame_command_buffers[vk->frame], VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk->pipeline_layouts[vk->pipeline], 5, 1, &u1alloc.model2_descriptor_set,
      1, &u2alloc.offset32);

  for (i = 0; i < mesh->primitive_count; ++i) {
    VkDescriptorSet sets[4];
    struct owl_model_primitive const *primitive;
    struct owl_model_material const *material;
    struct owl_model_texture const *base_color_texture;
    struct owl_model_texture const *normal_texture;
    struct owl_model_texture const *physical_desc_texture;
    struct owl_model_image const *base_color_image;
    struct owl_model_image const *normal_image;
    struct owl_model_image const *physical_desc_image;
    struct owl_model_material_push_constant push_constant;

    primitive = &model->primitives[mesh->primitives[i]];

    if (!primitive->count) {
      continue;
    }

    material = &model->materials[primitive->material];

    base_color_texture = &model->textures[material->base_color_texture];
    normal_texture = &model->textures[material->normal_texture];
    physical_desc_texture = &model->textures[material->physical_desc_texture];

    base_color_image = &model->images[base_color_texture->image];
    normal_image = &model->images[normal_texture->image];
    physical_desc_image = &model->images[physical_desc_texture->image];

    sets[0] = base_color_image->image.descriptor_set;
    sets[1] = normal_image->image.descriptor_set;
    sets[2] = physical_desc_image->image.descriptor_set;
    sets[3] = skin->ssbo_sets[vk->frame];

    vkCmdBindDescriptorSets(vk->frame_command_buffers[vk->frame],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk->pipeline_layouts[vk->pipeline], 1,
                            owl_array_size(sets), sets, 0, NULL);

    push_constant.base_color_factor[0] = material->base_color_factor[0];
    push_constant.base_color_factor[1] = material->base_color_factor[1];
    push_constant.base_color_factor[2] = material->base_color_factor[2];
    push_constant.base_color_factor[3] = material->base_color_factor[3];

    push_constant.emissive_factor[0] = 0.0F;
    push_constant.emissive_factor[1] = 0.0F;
    push_constant.emissive_factor[2] = 0.0F;
    push_constant.emissive_factor[3] = 0.0F;

    push_constant.diffuse_factor[0] = 0.0F;
    push_constant.diffuse_factor[1] = 0.0F;
    push_constant.diffuse_factor[2] = 0.0F;
    push_constant.diffuse_factor[3] = 0.0F;

    push_constant.specular_factor[0] = 0.0F;
    push_constant.specular_factor[1] = 0.0F;
    push_constant.specular_factor[2] = 0.0F;
    push_constant.specular_factor[3] = 0.0F;

    push_constant.workflow = 0;

    /* FIXME(samuel): add uv sets in material */
    if (OWL_MODEL_TEXTURE_NONE == material->base_color_texture) {
      push_constant.base_color_uv_set = -1;
    } else {
      push_constant.base_color_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->physical_desc_texture) {
      push_constant.physical_desc_uv_set = -1;
    } else {
      push_constant.physical_desc_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->normal_texture) {
      push_constant.normal_uv_set = -1;
    } else {
      push_constant.normal_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->occlusion_texture) {
      push_constant.occlusion_uv_set = -1;
    } else {
      push_constant.occlusion_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->emissive_texture) {
      push_constant.emissive_uv_set = -1;
    } else {
      push_constant.emissive_uv_set = 0;
    }

    push_constant.metallic_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.alpha_mask = 0.0F;
    push_constant.alpha_mask_cutoff = material->alpha_cutoff;

    vkCmdPushConstants(vk->frame_command_buffers[vk->frame],
                       vk->pipeline_layouts[vk->pipeline],
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constant),
                       &push_constant);

    vkCmdDrawIndexed(vk->frame_command_buffers[vk->frame], primitive->count, 1,
                     primitive->first, 0, 0);
  }

  return OWL_OK;
}

owl_public enum owl_code
owl_vk_draw_model(struct owl_vk_renderer *vk, struct owl_model const *model,
                  owl_m4 const matrix)
{
  owl_i32 i;

  owl_u64 offset = 0;
  enum owl_code code = OWL_OK;

  code = owl_vk_bind_pipeline(vk, OWL_VK_PIPELINE_MODEL);
  if (OWL_OK != code)
    return code;

  vkCmdBindVertexBuffers(vk->frame_command_buffers[vk->frame], 0, 1,
                         &model->vk_vertex_buffer, &offset);

  vkCmdBindIndexBuffer(vk->frame_command_buffers[vk->frame],
                       model->vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

  for (i = 0; i < model->root_count; ++i) {
    owl_model_node_id const root = model->roots[i];
    code = owl_vk_draw_model_node(vk, root, model, matrix);
    if (OWL_OK != code)
      return code;
  }

  return OWL_OK;
}

owl_public enum owl_code
owl_vk_draw_skybox(struct owl_vk_renderer *vk)
{
  owl_byte *data;
  struct owl_vk_frame_allocation valloc;
  struct owl_vk_frame_allocation ialloc;
  struct owl_vk_frame_allocation ualloc;

  VkDescriptorSet sets[2];
  VkCommandBuffer command_buffer;

  struct owl_pvm_ubo ubo;

  /*
   *    4----5
   *  / |  / |
   * 0----1  |
   * |  | |  |
   * |  6-|--7
   * | /  | /
   * 2----3
   */
  owl_local_persist struct owl_p_vertex const vertices[] = {
      {-1.0F, -1.0F, -1.0F}, /* 0 */
      {1.0F, -1.0F, -1.0F},  /* 1 */
      {-1.0F, 1.0F, -1.0F},  /* 2 */
      {1.0F, 1.0F, -1.0F},   /* 3 */
      {-1.0F, -1.0F, 1.0F},  /* 4 */
      {1.0F, -1.0F, 1.0F},   /* 5 */
      {-1.0F, 1.0F, 1.0F},   /* 6 */
      {1.0F, 1.0F, 1.0F}};   /* 7 */

  owl_local_persist owl_u32 const indices[] = {
      2, 3, 1, 1, 0, 2,  /* face 0 ....*/
      3, 7, 5, 5, 1, 3,  /* face 1 */
      6, 2, 0, 0, 4, 6,  /* face 2 */
      7, 6, 4, 4, 5, 7,  /* face 3 */
      3, 2, 6, 6, 7, 3,  /* face 4 */
      4, 0, 1, 1, 5, 4}; /* face 5 */

  owl_vk_bind_pipeline(vk, OWL_VK_PIPELINE_SKYBOX);

  command_buffer = vk->frame_command_buffers[vk->frame];

  data = owl_vk_frame_alloc(vk, sizeof(vertices), &valloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, vertices, sizeof(vertices));

  data = owl_vk_frame_alloc(vk, sizeof(indices), &ialloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, indices, sizeof(indices));

  owl_m4_copy(vk->projection, ubo.projection);
  owl_m4_identity(ubo.view);
  owl_m3_copy(vk->view, ubo.view);
  owl_m4_identity(ubo.model);

  data = owl_vk_frame_alloc(vk, sizeof(ubo), &ualloc);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  owl_memcpy(data, &ubo, sizeof(ubo));

  sets[0] = ualloc.pvm_descriptor_set;
  sets[1] = vk->skybox_descriptor_set;

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &valloc.buffer, &valloc.offset);

  vkCmdBindIndexBuffer(command_buffer, ialloc.buffer, ialloc.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vk->pipeline_layouts[vk->pipeline], 0,
                          owl_array_size(sets), sets, 1, &ualloc.offset32);

  vkCmdDrawIndexed(command_buffer, owl_array_size(indices), 1, 0, 0, 0);

  return OWL_OK;
}

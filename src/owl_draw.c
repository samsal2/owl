#include "owl_draw.h"

#include "owl_cloth_simulation.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#include <stdio.h>

OWL_PUBLIC owl_code
owl_draw_quad(struct owl_renderer *renderer, struct owl_quad const *quad,
              owl_m4 const matrix) {
  uint8_t *data;
  VkDescriptorSet descriptor_sets[2];
  VkCommandBuffer command_buffer;
  struct owl_pvm_uniform uniform;
  struct owl_pcu_vertex vertices[4];
  struct owl_vertex_allocation vertex_allocation;
  struct owl_index_allocation index_allocation;
  struct owl_uniform_allocation uniform_allocation;
  static uint32_t const indices[] = {2, 3, 1, 1, 0, 2};

  command_buffer = renderer->submit_command_buffers[renderer->frame];

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

  OWL_M4_COPY(renderer->projection, uniform.projection);
  OWL_M4_COPY(renderer->view, uniform.view);
  OWL_M4_COPY(matrix, uniform.model);

  data = owl_renderer_vertex_allocate(renderer, sizeof(vertices),
                                      &vertex_allocation);
  OWL_ASSERT(data);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, vertices, sizeof(vertices));

  data = owl_renderer_index_allocate(renderer, sizeof(indices),
                                     &index_allocation);
  OWL_ASSERT(data);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, indices, sizeof(indices));

  data = owl_renderer_uniform_allocate(renderer, sizeof(uniform),
                                       &uniform_allocation);
  OWL_ASSERT(data);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, &uniform, sizeof(uniform));

  descriptor_sets[0] = uniform_allocation.common_descriptor_set;
  descriptor_sets[1] = quad->texture->descriptor_set;

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
                         &vertex_allocation.offset);

  vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
                       index_allocation.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          renderer->common_pipeline_layout, 0,
                          OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets, 1,
                          &uniform_allocation.offset);

  vkCmdDrawIndexed(command_buffer, OWL_ARRAY_SIZE(indices), 1, 0, 0, 0);

  return OWL_OK;
}

static owl_code
owl_draw_glyph(struct owl_renderer *renderer, struct owl_glyph *glyph,
               owl_v3 const color) {
  owl_m4 matrix;
  owl_v3 scale;
  struct owl_quad quad;

  scale[0] = 2.0F / (float)renderer->height;
  scale[1] = 2.0F / (float)renderer->height;
  scale[2] = 2.0F / (float)renderer->height;

  OWL_V4_IDENTITY(matrix);
  owl_m4_scale_v3(matrix, scale, matrix);

  quad.texture = &renderer->font_atlas;

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

  return owl_draw_quad(renderer, &quad, matrix);
}

OWL_PUBLIC owl_code
owl_draw_text(struct owl_renderer *renderer, char const *text,
              owl_v3 const position, owl_v3 const color) {
  char const *letter;
  owl_v2 offset;
  owl_code code;
  VkCommandBuffer command_buffer;

  command_buffer = renderer->submit_command_buffers[renderer->frame];

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->text_pipeline);

  offset[0] = position[0] * (float)renderer->width;
  offset[1] = position[1] * (float)renderer->height;

  for (letter = &text[0]; '\0' != *letter; ++letter) {
    struct owl_glyph glyph;

    code = owl_renderer_fill_glyph(renderer, *letter, offset, &glyph);
    if (code)
      return code;

    code = owl_draw_glyph(renderer, &glyph, color);
    if (code)
      return code;
  }

  return OWL_OK;
}

static owl_code
owl_draw_model_node(struct owl_renderer *renderer, owl_model_node_id id,
                    struct owl_model const *model, owl_m4 const matrix) {
  int i;
  uint8_t *data;
  owl_model_node_id parent;
  VkCommandBuffer command_buffer;
  struct owl_model_node const *node;
  struct owl_model_mesh const *mesh;
  struct owl_model_skin const *skin;
  struct owl_model_skin_ssbo *ssbo;
  struct owl_model_uniform uniform;
  struct owl_uniform_allocation uniform1_allocation;
  owl_code code;

  command_buffer = renderer->submit_command_buffers[renderer->frame];

  node = &model->nodes[id];

  for (i = 0; i < node->child_count; ++i) {
    code = owl_draw_model_node(renderer, node->children[i], model, matrix);
    if (OWL_OK != code)
      return code;
  }

  if (OWL_MODEL_MESH_NONE == node->mesh)
    return OWL_OK;

  mesh = &model->meshes[node->mesh];
  skin = &model->skins[node->skin];
  ssbo = skin->ssbos[renderer->frame];

  OWL_M4_COPY(node->matrix, ssbo->matrix);

  for (parent = node->parent; OWL_MODEL_NODE_NONE != parent;
       parent = model->nodes[parent].parent)
    owl_m4_multiply(model->nodes[parent].matrix, ssbo->matrix, ssbo->matrix);

  OWL_M4_COPY(renderer->projection, uniform.projection);
  OWL_M4_COPY(renderer->view, uniform.view);
  OWL_M4_COPY(matrix, uniform.model);
  OWL_V4_ZERO(uniform.camera_position);
  OWL_V4_ZERO(uniform.light_direction);
  OWL_V4_ZERO(uniform.light_position);
  uniform.exposure = 0;
  uniform.gamma = 0;
  uniform.prefiltered_cube_mips = 0;
  uniform.scale_ibl_ambient = 0;
  uniform.debug_view_inputs = 0;
  uniform.debug_view_equation = 0;

  data = owl_renderer_uniform_allocate(renderer, sizeof(uniform),
                                       &uniform1_allocation);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, &uniform, sizeof(uniform));

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          renderer->model_pipeline_layout, 0, 1,
                          &uniform1_allocation.model_descriptor_set, 1,
                          &uniform1_allocation.offset);

  for (i = 0; i < mesh->primitive_count; ++i) {
    VkDescriptorSet sets[2];
    struct owl_model_primitive const *primitive;
    struct owl_model_material_push_constant push_constant;
    struct owl_model_material const *material;

    primitive = &model->primitives[mesh->primitives[i]];

    if (!primitive->count) {
      continue;
    }

    material = &model->materials[primitive->material];

    sets[0] = material->descriptor_set;
    sets[1] = skin->ssbo_sets[renderer->frame];

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            renderer->model_pipeline_layout, 1,
                            OWL_ARRAY_SIZE(sets), sets, 0, NULL);

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

    vkCmdPushConstants(command_buffer, renderer->model_pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constant),
                       &push_constant);

    vkCmdDrawIndexed(command_buffer, primitive->count, 1, primitive->first, 0,
                     0);
  }

  return OWL_OK;
}

OWL_PUBLIC owl_code
owl_draw_model(struct owl_renderer *renderer, struct owl_model const *model,
               owl_m4 const matrix) {
  int i;

  uint64_t offset = 0;
  owl_code code = OWL_OK;
  VkCommandBuffer command_buffer;

  command_buffer = renderer->submit_command_buffers[renderer->frame];

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->model_pipeline);

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &model->vertex_buffer, &offset);

  vkCmdBindIndexBuffer(command_buffer, model->index_buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  for (i = 0; i < model->root_count; ++i) {
    owl_model_node_id const root = model->roots[i];
    code = owl_draw_model_node(renderer, root, model, matrix);
    if (OWL_OK != code)
      return code;
  }

  return OWL_OK;
}

OWL_PUBLIC owl_code
owl_draw_skybox(struct owl_renderer *renderer) {
  uint8_t *data;
  struct owl_vertex_allocation vertex_allocation;
  struct owl_index_allocation index_allocation;
  struct owl_uniform_allocation uniform_allocation;
  VkDescriptorSet descriptor_sets[2];
  VkCommandBuffer command_buffer;
  struct owl_pvm_uniform uniform;
  /*
   *    4----5
   *  / |  / |
   * 0----1  |
   * |  | |  |
   * |  6-|--7
   * | /  | /
   * 2----3
   */
  static struct owl_p_vertex const vertices[] = {{-1.0F, -1.0F, -1.0F}, /* 0 */
                                                 {1.0F, -1.0F, -1.0F},  /* 1 */
                                                 {-1.0F, 1.0F, -1.0F},  /* 2 */
                                                 {1.0F, 1.0F, -1.0F},   /* 3 */
                                                 {-1.0F, -1.0F, 1.0F},  /* 4 */
                                                 {1.0F, -1.0F, 1.0F},   /* 5 */
                                                 {-1.0F, 1.0F, 1.0F},   /* 6 */
                                                 {1.0F, 1.0F, 1.0F}};   /* 7 */
  static uint32_t const indices[] = {2, 3, 1, 1, 0, 2,  /* face 0 ....*/
                                     3, 7, 5, 5, 1, 3,  /* face 1 */
                                     6, 2, 0, 0, 4, 6,  /* face 2 */
                                     7, 6, 4, 4, 5, 7,  /* face 3 */
                                     3, 2, 6, 6, 7, 3,  /* face 4 */
                                     4, 0, 1, 1, 5, 4}; /* face 5 */

  command_buffer = renderer->submit_command_buffers[renderer->frame];

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->skybox_pipeline);

  data = owl_renderer_vertex_allocate(renderer, sizeof(vertices),
                                      &vertex_allocation);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, vertices, sizeof(vertices));

  data = owl_renderer_index_allocate(renderer, sizeof(indices),
                                     &index_allocation);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, indices, sizeof(indices));

  OWL_M4_COPY(renderer->projection, uniform.projection);
  OWL_V4_IDENTITY(uniform.view);
  OWL_M3_COPY(renderer->view, uniform.view);
  OWL_V4_IDENTITY(uniform.model);

  data = owl_renderer_uniform_allocate(renderer, sizeof(uniform),
                                       &uniform_allocation);
  if (!data)
    return OWL_ERROR_NO_FRAME_MEMORY;
  OWL_MEMCPY(data, &uniform, sizeof(uniform));

  descriptor_sets[0] = uniform_allocation.common_descriptor_set;
  descriptor_sets[1] = renderer->skybox.descriptor_set;

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
                         &vertex_allocation.offset);

  vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
                       index_allocation.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          renderer->common_pipeline_layout, 0,
                          OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets, 1,
                          &uniform_allocation.offset);

  vkCmdDrawIndexed(command_buffer, OWL_ARRAY_SIZE(indices), 1, 0, 0, 0);

  return OWL_OK;
}

OWL_PUBLIC owl_code
owl_draw_renderer_state(struct owl_renderer *renderer) {
  char buffer[256];
  owl_v3 position = {-0.8F, -0.8F, 0.0F};
  owl_v3 color = {1.0F, 1.0F, 1.0F};
  static double previous_time = 0.0;
  static double current_time = 0.0;
  double fps = 0.0;

  previous_time = current_time;
  current_time = owl_plataform_get_time(renderer->plataform);
  fps = 1.0 / (current_time - previous_time);

  snprintf(buffer, sizeof(buffer), "fps: %.2f", fps);

  owl_draw_text(renderer, buffer, position, color);

  position[1] += 0.05F;

  snprintf(buffer, sizeof(buffer), "vertex_buffer_size: %llu",
           renderer->vertex_buffer_size);

  owl_draw_text(renderer, buffer, position, color);

  position[1] += 0.05F;

  snprintf(buffer, sizeof(buffer), "index_buffer_size: %llu",
           renderer->index_buffer_size);

  owl_draw_text(renderer, buffer, position, color);

  position[1] += 0.05F;

  snprintf(buffer, sizeof(buffer), "uniform_buffer_size: %llu",
           renderer->uniform_buffer_size);

  owl_draw_text(renderer, buffer, position, color);

  position[1] += 0.05F;

  snprintf(buffer, sizeof(buffer), "upload_buffer_size: %llu",
           renderer->upload_buffer_size);

  owl_draw_text(renderer, buffer, position, color);

  return OWL_OK;
}

OWL_PUBLIC owl_code
owl_draw_cloth_simulation(struct owl_renderer *renderer,
                          struct owl_cloth_simulation *sim) {
  int32_t i;
  int32_t j;

  uint32_t index_count;
  uint32_t *indices;
  struct owl_index_allocation index_allocation;

  uint64_t vertex_count;
  struct owl_pcu_vertex *vertices;
  struct owl_vertex_allocation vertex_allocation;

  VkDescriptorSet descriptor_sets[2];
  struct owl_pvm_uniform *uniform;
  struct owl_uniform_allocation uniform_allocation;

  VkCommandBuffer command_buffer;

  command_buffer = renderer->submit_command_buffers[renderer->frame];

  index_count = (sim->width - 1) * (sim->height - 1) * 6;
  indices = owl_renderer_index_allocate(
      renderer, index_count * sizeof(*indices), &index_allocation);
  if (!indices)
    return OWL_ERROR_NO_FRAME_MEMORY;

  for (i = 0; i < (sim->height - 1); ++i) {
    for (j = 0; j < (sim->width - 1); ++j) {
      int32_t const k = i * sim->width + j;
      int32_t const fixed_k = (i * (sim->width - 1) + j) * 6;

      indices[fixed_k + 0] = k + sim->width;
      indices[fixed_k + 1] = k + sim->width + 1;
      indices[fixed_k + 2] = k + 1;
      indices[fixed_k + 3] = k + 1;
      indices[fixed_k + 4] = k;
      indices[fixed_k + 5] = k + sim->width;
    }
  }

  vertex_count = sim->width * sim->height;
  vertices = owl_renderer_vertex_allocate(
      renderer, vertex_count * sizeof(*vertices), &vertex_allocation);
  if (!vertices)
    return OWL_ERROR_NO_FRAME_MEMORY;

  for (i = 0; i < sim->height; ++i) {
    for (j = 0; j < sim->width; ++j) {
      int32_t k = i * sim->width + j;

      struct owl_cloth_particle *particle = &sim->particles[k];
      struct owl_pcu_vertex *vertex = &vertices[k];

      OWL_V3_COPY(particle->position, vertex->position);
      OWL_V3_SET(vertex->color, 1.0F, 1.0F, 1.0F);
      vertex->uv[0] = (float)j / (float)sim->width;
      vertex->uv[1] = (float)i / (float)sim->height;
    }
  }

  uniform = owl_renderer_uniform_allocate(renderer, sizeof(*uniform),
                                          &uniform_allocation);
  if (!uniform)
    return OWL_ERROR_NO_FRAME_MEMORY;

  OWL_M4_COPY(renderer->projection, uniform->projection);
  OWL_M4_COPY(renderer->view, uniform->view);
  OWL_M4_COPY(sim->model, uniform->model);

  descriptor_sets[0] = uniform_allocation.common_descriptor_set;
  descriptor_sets[1] = sim->material.descriptor_set;

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->basic_pipeline);

  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_allocation.buffer,
                         &vertex_allocation.offset);

  vkCmdBindIndexBuffer(command_buffer, index_allocation.buffer,
                       index_allocation.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          renderer->common_pipeline_layout, 0,
                          OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets, 1,
                          &uniform_allocation.offset);

  vkCmdDrawIndexed(command_buffer, index_count, 1, 0, 0, 0);

  return OWL_OK;
}

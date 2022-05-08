#include "owl_draw_command.h"

#include "owl_camera.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

enum owl_code
owl_draw_command_basic_submit(struct owl_draw_command_basic const *cmd,
                              struct owl_renderer *r,
                              struct owl_camera const *cam) {
  owl_u64 sz;
  VkDescriptorSet sets[2];
  struct owl_draw_command_uniform ubo;
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  struct owl_renderer_frame_heap_reference uref;
  enum owl_code code = OWL_SUCCESS;

  sz = (owl_u64)cmd->vertex_count * sizeof(*cmd->vertices);
  code = owl_renderer_frame_heap_submit(r, sz, cmd->vertices, &vref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  sz = (owl_u64)cmd->indice_count * sizeof(*cmd->indices);
  code = owl_renderer_frame_heap_submit(r, sz, cmd->indices, &iref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  OWL_M4_COPY(cam->projection, ubo.projection);
  OWL_M4_COPY(cam->view, ubo.view);
  OWL_M4_COPY(cmd->model, ubo.model);

  sz = sizeof(ubo);
  code = owl_renderer_frame_heap_submit(r, sz, &ubo, &uref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  sets[0] = uref.common_ubo_set;
  sets[1] = r->image_pool_sets[cmd->image];

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &vref.buffer,
                         &vref.offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, iref.buffer, iref.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &uref.offset32);

  vkCmdDrawIndexed(r->active_frame_command_buffer, cmd->indice_count, 1, 0, 0,
                   0);

out:
  return code;
}

enum owl_code
owl_draw_command_quad_submit(struct owl_draw_command_quad const *cmd,
                             struct owl_renderer *r,
                             struct owl_camera const *cam) {
  owl_u64 sz;
  VkDescriptorSet sets[2];
  struct owl_draw_command_uniform ubo;
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  struct owl_renderer_frame_heap_reference uref;
  enum owl_code code = OWL_SUCCESS;
  owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  sz = OWL_ARRAY_SIZE(cmd->vertices) * sizeof(cmd->vertices[0]);
  code = owl_renderer_frame_heap_submit(r, sz, cmd->vertices, &vref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  sz = OWL_ARRAY_SIZE(indices) * sizeof(indices[0]);
  code = owl_renderer_frame_heap_submit(r, sz, indices, &iref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  OWL_M4_COPY(cam->projection, ubo.projection);
  OWL_M4_COPY(cam->view, ubo.view);
  OWL_M4_COPY(cmd->model, ubo.model);

  sz = sizeof(ubo);
  code = owl_renderer_frame_heap_submit(r, sz, &ubo, &uref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  sets[0] = uref.common_ubo_set;
  sets[1] = r->image_pool_sets[cmd->image];

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &vref.buffer,
                         &vref.offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, iref.buffer, iref.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &uref.offset32);

  vkCmdDrawIndexed(r->active_frame_command_buffer, OWL_ARRAY_SIZE(indices), 1,
                   0, 0, 0);
out:
  return code;
}

/* FIXME(samuel): scaling doesn't work right */
enum owl_code
owl_draw_command_text_submit(struct owl_draw_command_text const *cmd,
                             struct owl_renderer *r,
                             struct owl_camera const *cam) {
  char const *l;
  owl_v3 offset;
  owl_v3 scale;
  enum owl_code code = OWL_SUCCESS;

#if 0
  /* FIXME(samuel): make sure scaling is right */
  offset[0] = cmd->position[0] * r->window_height;
  offset[1] = cmd->position[1] * r->window_height;

  /* HACK(samuel): using framebuffer height to scale the z axis */
  offset[2] = cmd->position[2];
#endif

  offset[0] = cmd->position[0] * (float)r->framebuffer_width;
  offset[1] = cmd->position[1] * (float)r->framebuffer_height;
  offset[2] = 0.0F;

  scale[0] = cmd->scale / (float)r->window_height;
  scale[1] = cmd->scale / (float)r->window_height;
  scale[2] = cmd->scale / (float)r->window_height;

  for (l = cmd->text; '\0' != *l; ++l) {
    owl_v3 p0;
    owl_v3 p1;
    owl_v3 p2;
    owl_v3 p3;
    struct owl_renderer_font_glyph glyph;
    struct owl_draw_command_quad quad;

    owl_renderer_active_font_fill_glyph(r, *l, offset, &glyph);

    quad.image = r->font_pool_atlases[r->active_font];

    OWL_ASSERT(r->image_pool_slots[quad.image]);

    OWL_M4_IDENTITY(quad.model);
    owl_m4_scale(quad.model, scale, quad.model);

    OWL_V3_COPY(glyph.positions[0], p0);
    OWL_V3_COPY(glyph.positions[1], p1);
    OWL_V3_COPY(glyph.positions[2], p2);
    OWL_V3_COPY(glyph.positions[3], p3);

    OWL_V3_COPY(p0, quad.vertices[0].position);
    OWL_V3_COPY(cmd->color, quad.vertices[0].color);
    OWL_V2_COPY(glyph.uvs[0], quad.vertices[0].uv);

    OWL_V3_COPY(p1, quad.vertices[1].position);
    OWL_V3_COPY(cmd->color, quad.vertices[1].color);
    OWL_V2_COPY(glyph.uvs[1], quad.vertices[1].uv);

    OWL_V3_COPY(p2, quad.vertices[2].position);
    OWL_V3_COPY(cmd->color, quad.vertices[2].color);
    OWL_V2_COPY(glyph.uvs[2], quad.vertices[2].uv);

    OWL_V3_COPY(p3, quad.vertices[3].position);
    OWL_V3_COPY(cmd->color, quad.vertices[3].color);
    OWL_V2_COPY(glyph.uvs[3], quad.vertices[3].uv);

    owl_draw_command_quad_submit(&quad, r, cam);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

OWL_INTERNAL enum owl_code owl_draw_command_model_node_submit(
    struct owl_draw_command_model const *cmd, struct owl_renderer *r,
    struct owl_camera const *cam, owl_model_node_descriptor node) {
  owl_i32 i;
  owl_model_node_descriptor parent;
  struct owl_model const *model;
  struct owl_model_uniform ubo;
  struct owl_model_uniform_params ubo_params;
  struct owl_model_node const *nd;
  struct owl_model_mesh const *md;
  struct owl_model_skin const *sd;
  struct owl_model_skin_ssbo *ssbo;

  /* TODO(samuel): materials */
#if 0
  struct owl_model_push_constant push_constant;
#endif
  enum owl_code code = OWL_SUCCESS;

  model = cmd->skin;
  nd = &model->nodes[node];

  for (i = 0; i < nd->child_count; ++i) {
    code = owl_draw_command_model_node_submit(cmd, r, cam, nd->children[i]);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  if (OWL_MODEL_MESH_NONE == nd->mesh) {
    goto out;
  }

  md = &model->meshes[nd->mesh];

  if (!md->primitive_count) {
    goto out;
  }

  sd = &model->skins[nd->skin];
  ssbo = sd->ssbo_datas[r->active_frame];

  OWL_M4_COPY(model->nodes[node].matrix, ssbo->matrix);

  for (parent = model->nodes[node].parent; OWL_MODEL_NODE_PARENT_NONE != parent;
       parent = model->nodes[parent].parent) {
    owl_m4_multiply(model->nodes[parent].matrix, ssbo->matrix, ssbo->matrix);
  }

  /* FIXME(samuel): find a way to make this global */
  OWL_M4_COPY(cmd->model, ubo.model);
  OWL_V4_ZERO(ubo_params.light_direction);

  for (i = 0; i < md->primitive_count; ++i) {
    owl_u32 offsets[2];
    VkDescriptorSet sets[6];
    owl_model_primitive_descriptor primitive;
    struct owl_renderer_frame_heap_reference uniform_reference;
    struct owl_renderer_frame_heap_reference uniform_params_reference;
    owl_model_material_descriptor material;
    owl_model_texture_descriptor base_color_texture;
    owl_model_texture_descriptor normal_texture;
    owl_model_texture_descriptor physical_desc_texture;
    owl_model_image_descriptor base_color_image;
    owl_model_image_descriptor normal_image;
    owl_model_image_descriptor physical_desc_image;
    struct owl_model_primitive const *primitive_data;
    struct owl_model_material const *material_data;
    struct owl_model_texture const *base_color_texture_data;
    struct owl_model_texture const *normal_texture_data;
    struct owl_model_texture const *physical_desc_texture_data;
    struct owl_model_image const *base_color_image_data;
    struct owl_model_image const *normal_image_data;
    struct owl_model_image const *physical_desc_image_data;
    struct owl_model_material_push_constant push_constant;

    primitive = md->primitives[i];
    primitive_data = &model->primitives[primitive];

    if (!primitive_data->count) {
      continue;
    }

    material = primitive_data->material;
    material_data = &model->materials[material];

    base_color_texture = material_data->base_color_texture;
    base_color_texture_data = &model->textures[base_color_texture];

    normal_texture = material_data->normal_texture;
    normal_texture_data = &model->textures[normal_texture];

    physical_desc_texture = material_data->physical_desc_texture;
    physical_desc_texture_data = &model->textures[physical_desc_texture];

    base_color_image = base_color_texture_data->model_image;
    base_color_image_data = &model->images[base_color_image];

    normal_image = normal_texture_data->model_image;
    normal_image_data = &model->images[normal_image];

    physical_desc_image = physical_desc_texture_data->model_image;
    physical_desc_image_data = &model->images[physical_desc_image];

    OWL_M4_COPY(cam->projection, ubo.projection);
    OWL_M4_COPY(cam->view, ubo.view);
    OWL_V4_ZERO(ubo.light);
    OWL_V3_COPY(cmd->light, ubo.light);

    code = owl_renderer_frame_heap_submit(r, sizeof(ubo), &ubo,
                                          &uniform_reference);

    if (OWL_SUCCESS != code) {
      goto out;
    }

    code = owl_renderer_frame_heap_submit(r, sizeof(ubo_params), &ubo,
                                          &uniform_params_reference);

    if (OWL_SUCCESS != code) {
      goto out;
    }

    /* TODO(samuel): generic pipeline layout ordered by frequency */
    sets[0] = uniform_reference.model_ubo_set;
    sets[1] = r->image_pool_sets[base_color_image_data->renderer_image];
    sets[2] = r->image_pool_sets[normal_image_data->renderer_image];
    sets[3] = r->image_pool_sets[physical_desc_image_data->renderer_image];
    sets[4] = sd->ssbo_sets[r->active_frame];
    sets[5] = uniform_params_reference.model_ubo_params_set;

    offsets[0] = uniform_reference.offset32;
    offsets[1] = uniform_params_reference.offset32;

#if 1
    OWL_V4_COPY(material_data->base_color_factor,
                push_constant.base_color_factor);
    OWL_V4_SET(0.0F, 0.0F, 0.0F, 0.0F, push_constant.emissive_factor);
    OWL_V4_SET(0.0F, 0.0F, 0.0F, 0.0F, push_constant.diffuse_factor);
    OWL_V4_SET(0.0F, 0.0F, 0.0F, 0.0F, push_constant.specular_factor);

    push_constant.workflow = 0;

    /* FIXME(samuel): add uv sets in material */
    if (OWL_MODEL_TEXTURE_NONE == material_data->base_color_texture) {
      push_constant.base_color_uv_set = -1;
    } else {
      push_constant.base_color_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material_data->physical_desc_texture) {
      push_constant.physical_desc_uv_set = -1;
    } else {
      push_constant.physical_desc_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material_data->normal_texture) {
      push_constant.normal_uv_set = -1;
    } else {
      push_constant.normal_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material_data->occlusion_texture) {
      push_constant.occlusion_uv_set = -1;
    } else {
      push_constant.occlusion_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material_data->emissive_texture) {
      push_constant.emissive_uv_set = -1;
    } else {
      push_constant.emissive_uv_set = 0;
    }

    push_constant.metallic_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.alpha_mask = 0.0F;
    push_constant.alpha_mask_cutoff = material_data->alpha_cutoff;

    vkCmdPushConstants(r->active_frame_command_buffer,
                       r->active_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(push_constant), &push_constant);
#endif

    vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                            sets, OWL_ARRAY_SIZE(offsets), offsets);

    vkCmdDrawIndexed(r->active_frame_command_buffer, primitive_data->count, 1,
                     primitive_data->first, 0, 0);
  }

out:
  return code;
}

enum owl_code
owl_draw_command_model_submit(struct owl_draw_command_model const *cmd,
                              struct owl_renderer *r,
                              struct owl_camera const *cam) {
  owl_i32 i;
  owl_u64 offset = 0;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model const *model = cmd->skin;

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1,
                         &model->vertices_buffer, &offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, model->indices_buffer, 0,
                       VK_INDEX_TYPE_UINT32);

  for (i = 0; i < cmd->skin->root_count; ++i) {
    code = owl_draw_command_model_node_submit(cmd, r, cam, model->roots[i]);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

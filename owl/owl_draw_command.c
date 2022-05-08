#include "owl_draw_command.h"

#include "owl_camera.h"
#include "owl_font.h"
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

  sz = (owl_u64)cmd->vertices_count * sizeof(*cmd->vertices);
  code = owl_renderer_frame_heap_submit(r, sz, cmd->vertices, &vref);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  sz = (owl_u64)cmd->indices_count * sizeof(*cmd->indices);
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
  sets[1] = r->image_pool_sets[cmd->image.slot];

  vkCmdBindVertexBuffers(r->active_frame_command_buffer, 0, 1, &vref.buffer,
                         &vref.offset);

  vkCmdBindIndexBuffer(r->active_frame_command_buffer, iref.buffer, iref.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(r->active_frame_command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r->active_pipeline_layout, 0, OWL_ARRAY_SIZE(sets),
                          sets, 1, &uref.offset32);

  vkCmdDrawIndexed(r->active_frame_command_buffer, cmd->indices_count, 1, 0, 0,
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
  sets[1] = r->image_pool_sets[cmd->image.slot];

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
    struct owl_font_glyph glyph;
    struct owl_draw_command_quad quad;

    owl_font_fill_glyph(cmd->font, *l, offset, &glyph);

    quad.image.slot = cmd->font->atlas.slot;

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
    struct owl_camera const *cam, struct owl_model_node const *node) {
  owl_i32 i;
  struct owl_model_node parent;
  struct owl_model const *model;
  struct owl_model_uniform ubo;
  struct owl_model_uniform_params ubo_params;
  struct owl_model_node_data const *node_data;
  struct owl_model_mesh_data const *mesh_data;
  struct owl_model_skin_data const *skin_data;
  struct owl_model_skin_ssbo_data *ssbo;

  /* TODO(samuel): materials */
#if 0
  struct owl_model_push_constant push_constant;
#endif
  enum owl_code code = OWL_SUCCESS;

  model = cmd->skin;
  node_data = &model->nodes[node->slot];

  for (i = 0; i < node_data->children_count; ++i) {
    code = owl_draw_command_model_node_submit(cmd, r, cam,
                                              &node_data->children[i]);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  if (OWL_MODEL_NODE_NO_MESH_SLOT == node_data->mesh.slot) {
    goto out;
  }

  mesh_data = &model->meshes[node_data->mesh.slot];

  if (!mesh_data->primitives_count) {
    goto out;
  }

  skin_data = &model->skins[node_data->skin.slot];
  ssbo = skin_data->ssbo_datas[r->active_frame];

  OWL_M4_COPY(model->nodes[node->slot].matrix, ssbo->matrix);

  for (parent.slot = model->nodes[node->slot].parent.slot;
       OWL_MODEL_NODE_NO_PARENT_SLOT != parent.slot;
       parent.slot = model->nodes[parent.slot].parent.slot) {
    owl_m4_multiply(model->nodes[parent.slot].matrix, ssbo->matrix,
                    ssbo->matrix);
  }

  /* FIXME(samuel): find a way to make this global */
  OWL_M4_COPY(cmd->model, ubo.model);
  OWL_V4_ZERO(ubo_params.light_direction);

  for (i = 0; i < mesh_data->primitives_count; ++i) {
    owl_u32 offsets[2];
    VkDescriptorSet sets[6];
    struct owl_model_primitive primitive;
    struct owl_renderer_frame_heap_reference uniform_reference;
    struct owl_renderer_frame_heap_reference uniform_params_reference;
    struct owl_model_material material;
    struct owl_model_texture base_color_texture;
    struct owl_model_texture normal_texture;
    struct owl_model_texture physical_desc_texture;
    struct owl_model_image base_color_image;
    struct owl_model_image normal_image;
    struct owl_model_image physical_desc_image;
    struct owl_model_primitive_data const *primitive_data;
    struct owl_model_material_data const *material_data;
    struct owl_model_texture_data const *base_color_texture_data;
    struct owl_model_texture_data const *normal_texture_data;
    struct owl_model_texture_data const *physical_desc_texture_data;
    struct owl_model_image_data const *base_color_image_data;
    struct owl_model_image_data const *normal_image_data;
    struct owl_model_image_data const *physical_desc_image_data;
    struct owl_model_material_push_constant push_constant;

    primitive.slot = mesh_data->primitives[i].slot;
    primitive_data = &model->primitives[primitive.slot];

    if (!primitive_data->count) {
      continue;
    }

    material.slot = primitive_data->material.slot;
    material_data = &model->materials[material.slot];

    base_color_texture.slot = material_data->base_color_texture.slot;
    base_color_texture_data = &model->textures[base_color_texture.slot];

    normal_texture.slot = material_data->normal_texture.slot;
    normal_texture_data = &model->textures[normal_texture.slot];

    physical_desc_texture.slot = material_data->physical_desc_texture.slot;
    physical_desc_texture_data = &model->textures[physical_desc_texture.slot];

    base_color_image.slot = base_color_texture_data->image.slot;
    base_color_image_data = &model->images[base_color_image.slot];

    normal_image.slot = normal_texture_data->image.slot;
    normal_image_data = &model->images[normal_image.slot];

    physical_desc_image.slot = physical_desc_texture_data->image.slot;
    physical_desc_image_data = &model->images[physical_desc_image.slot];

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
    sets[1] = r->image_pool_sets[base_color_image_data->image.slot];
    sets[2] = r->image_pool_sets[normal_image_data->image.slot];
    sets[3] = r->image_pool_sets[physical_desc_image_data->image.slot];
    sets[4] = skin_data->ssbo_sets[r->active_frame];
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
    if (OWL_MODEL_NO_TEXTURE_SLOT == material_data->base_color_texture.slot) {
      push_constant.base_color_uv_set = -1;
    } else {
      push_constant.base_color_uv_set = 0;
    }

    if (OWL_MODEL_NO_TEXTURE_SLOT ==
        material_data->physical_desc_texture.slot) {
      push_constant.physical_desc_uv_set = -1;
    } else {
      push_constant.physical_desc_uv_set = 0;
    }

    if (OWL_MODEL_NO_TEXTURE_SLOT == material_data->normal_texture.slot) {
      push_constant.normal_uv_set = -1;
    } else {
      push_constant.normal_uv_set = 0;
    }

    if (OWL_MODEL_NO_TEXTURE_SLOT == material_data->occlusion_texture.slot) {
      push_constant.occlusion_uv_set = -1;
    } else {
      push_constant.occlusion_uv_set = 0;
    }

    if (OWL_MODEL_NO_TEXTURE_SLOT == material_data->emissive_texture.slot) {
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

  for (i = 0; i < cmd->skin->roots_count; ++i) {
    code = owl_draw_command_model_node_submit(cmd, r, cam, &model->roots[i]);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

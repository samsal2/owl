#include "model.h"

#include "internal.h"
#include "renderer.h"
#include "texture.h"
#include "vulkan/vulkan_core.h"

#include <cgltf/cgltf.h>
#include <math.h>
#include <owl/owl.h>
#include <stb/stb_image.h>
#include <stdio.h>

struct owl_model_load_state {
  int cur_idx;
  owl_u32 *idxs;

  int cur_vtx;
  struct owl_model_vertex *vtxs;
};

OWL_INTERNAL enum owl_code owl_request_node_(struct owl_model *model,
                                             struct owl_model_node *node) {
  int const handle = model->nodes_count++;

  if (OWL_MODEL_MAX_NODES <= model->nodes_count)
    return OWL_ERROR_OUT_OF_SPACE;

  node->handle = handle;

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code owl_request_mesh_(struct owl_model *model,
                                             struct owl_model_mesh *mesh) {
  int const handle = model->meshes_count++;

  if (OWL_MODEL_MAX_MESHES <= model->meshes_count)
    return OWL_ERROR_OUT_OF_SPACE;

  mesh->handle = handle;

  return OWL_SUCCESS;
}

OWL_INTERNAL cgltf_attribute const *owl_find_attr_(cgltf_primitive const *prim,
                                                   char const *name) {
  int i;

  for (i = 0; i < (int)prim->attributes_count; ++i)
    if (0 == strcmp(prim->attributes[i].name, name))
      return &prim->attributes[i];

  return NULL;
}

OWL_INTERNAL void const *owl_get_attr_data_(cgltf_attribute const *attr) {
  owl_byte const *bdata = attr->data->buffer_view->buffer->data;
  return &bdata[attr->data->buffer_view->offset + attr->data->offset];
}

OWL_INTERNAL int owl_get_type_size_(cgltf_type type) {
  switch (type) {
  case cgltf_type_scalar:
    return 1;

  case cgltf_type_vec2:
    return 2;

  case cgltf_type_vec3:
    return 3;

  case cgltf_type_vec4:
    return 4;

  case cgltf_type_mat2:
    return 2 * 2;

  case cgltf_type_mat3:
    return 3 * 3;

  case cgltf_type_mat4:
    return 4 * 4;

  case cgltf_type_invalid:
    return 0;
  }
}

OWL_INTERNAL enum owl_code
owl_get_material_(struct owl_model const *model, char const *name,
                  struct owl_model_material *material) {
  int i;

  for (i = 0; i < model->materials_count; ++i) {
    if (0 == strcmp(model->materials[i].name, name)) {
      material->handle = i;
      return OWL_SUCCESS;
    }
  }

  return OWL_ERROR_UNKNOWN;
}

OWL_INTERNAL enum owl_code owl_process_primitive_(
    cgltf_primitive const *prim, struct owl_model const *model,
    struct owl_model_load_state *state, struct owl_model_primitive *out) {
  int i;
  cgltf_attribute const *attr;

  int pos_stride = 0;
  owl_byte const *pos_data = NULL;

  int normal_stride = 0;
  owl_byte const *normal_data = NULL;

  int uv0_stride = 0;
  owl_byte const *uv0_data = NULL;

  int uv1_stride = 0;
  owl_byte const *uv1_data = NULL;

  int joint_stride = 0;
  owl_byte const *joint_data = NULL;
  cgltf_component_type joint_type = cgltf_component_type_invalid;

  int weight_stride = 0;
  owl_byte const *weight_data = NULL;

  enum owl_code code = OWL_SUCCESS;

  out->first_index = (owl_u32)state->cur_idx;
  out->indices_count = (owl_u32)prim->indices->count;

  /* position */
  if ((attr = owl_find_attr_(prim, "POSITION"))) {
    pos_data = owl_get_attr_data_(attr);
    pos_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
    out->vertex_count = (owl_u32)attr->data->count;
  } else {
    return OWL_ERROR_UNKNOWN;
  }

  if ((attr = owl_find_attr_(prim, "NORMAL"))) {
    normal_data = owl_get_attr_data_(attr);
    normal_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_find_attr_(prim, "TEXCOORD_0"))) {
    uv0_data = owl_get_attr_data_(attr);
    uv0_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_find_attr_(prim, "TEXCOORD_1"))) {
    uv1_data = owl_get_attr_data_(attr);
    uv1_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_find_attr_(prim, "JOINTS_0"))) {
    joint_data = owl_get_attr_data_(attr);
    joint_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
    joint_type = attr->data->component_type;
  }

  if ((attr = owl_find_attr_(prim, "WEIGHTS_0"))) {
    weight_data = owl_get_attr_data_(attr);
    weight_stride = owl_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  /* vertices */
  for (i = 0; i < (int)out->vertex_count; ++i) {
    struct owl_model_vertex *v = &state->vtxs[state->cur_vtx + i];
    {
      float const *data = (float const *)&pos_data[i * pos_stride];
      v->pos[0] = data[0];
      v->pos[1] = -data[1]; /* FIXME(samuel): y coords are reversed */
      v->pos[2] = data[2];
    }

    if (normal_data) {
      float const *data = (float const *)&normal_data[i * normal_stride];
      v->normal[0] = data[0];
      v->normal[1] = data[1];
      v->normal[2] = data[2];
    } else {
      OWL_ZERO_V3(v->normal);
    }

    if (uv0_data) {
      float const *data = (float const *)&uv0_data[i * uv0_stride];
      v->uv0[0] = data[0];
      v->uv0[1] = data[1];
    } else {
      OWL_ZERO_V2(v->uv0);
    }

    if (uv1_data) {
      float const *data = (float const *)&uv1_data[i * uv1_stride];
      v->uv1[0] = data[0];
      v->uv1[1] = data[1];
    } else {
      OWL_ZERO_V2(v->uv1);
    }

    if (joint_data && weight_data) {
      switch (joint_type) {

      case cgltf_component_type_r_8: { /* BYTE */
        char const *data = (char const *)&joint_data[i * joint_stride];
        v->joint0[0] = data[0];
        v->joint0[1] = data[1];
        v->joint0[2] = data[2];
        v->joint0[3] = data[3];
      } break;

      case cgltf_component_type_r_16: { /* SHORT */
        short const *data = (short const *)&joint_data[i * joint_stride];
        v->joint0[0] = data[0];
        v->joint0[1] = data[1];
        v->joint0[2] = data[2];
        v->joint0[3] = data[3];
      } break;

        /* not supported by spec */
      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8u:  /* UNSIGNED_BYTE */
      case cgltf_component_type_r_16u: /* UNSIGNED_SHORT */
      case cgltf_component_type_r_32u: /* UNSIGNED_INT */
      case cgltf_component_type_r_32f: /* FLOAT */
      default:
        return OWL_ERROR_UNKNOWN;
      }
      {
        float const *data = (float const *)&weight_data[i * weight_stride];
        v->weight0[0] = data[0];
        v->weight0[1] = data[1];
        v->weight0[2] = data[2];
        v->weight0[3] = data[4];
      }
    } else {
      OWL_ZERO_V4(v->joint0);
      OWL_ZERO_V4(v->weight0);
    }
  }

  state->cur_vtx += (int)out->vertex_count;

  /* indices */
  {
    owl_byte const *idx_data = &(
        (owl_byte const *)prim->indices->buffer_view->buffer
            ->data)[prim->indices->buffer_view->offset + prim->indices->offset];

    switch (prim->indices->component_type) {
    case cgltf_component_type_r_8u: { /* UNSIGNED_BYTE */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->idxs[state->cur_idx + i] = ((owl_u8 const *)idx_data)[i];
      }
    } break;
    case cgltf_component_type_r_16u: { /* UNSIGNED_SHORT */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->idxs[state->cur_idx + i] = ((owl_u16 const *)idx_data)[i];
      }
    } break;
    case cgltf_component_type_r_32u: { /* UNSIGNED_INT */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->idxs[state->cur_idx + i] = ((owl_u32 const *)idx_data)[i];
      }
    } break;
      /* not supported by spec */
    case cgltf_component_type_invalid:
    case cgltf_component_type_r_8:   /* BYTE */
    case cgltf_component_type_r_16:  /* SHORT */
    case cgltf_component_type_r_32f: /* FLOAT */
    default:
      return OWL_ERROR_UNKNOWN;
    }
  }

  state->cur_idx += (int)prim->indices->count;

  code = owl_get_material_(model, prim->material->name, &out->material);

  if (OWL_SUCCESS != code)
    return code;

  return code;
}

OWL_INTERNAL enum owl_code
owl_process_mesh_(struct owl_vk_renderer const *renderer,
                  cgltf_mesh const *mesh, struct owl_model_node const *node,
                  struct owl_model_load_state *state, struct owl_model *model) {
  int i;
  enum owl_code code;
  struct owl_model_mesh_data *mesh_data;
  struct owl_model_node_data *node_data = &model->nodes[node->handle];

  code = owl_request_mesh_(model, &node_data->mesh);

  if (OWL_SUCCESS != code)
    goto end;

  mesh_data = &model->meshes[node_data->mesh.handle];

  if (OWL_MODEL_MAX_PRIMITIVES_PER_MESH <= mesh->primitives_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  mesh_data->primitives_count = (int)mesh->primitives_count;

  {
    VkBufferCreateInfo buffer;
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size = sizeof(struct owl_model_mesh_uniform_block);
    buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(
        vkCreateBuffer(renderer->device, &buffer, NULL, &mesh_data->ubo_buf));
  }

  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetBufferMemoryRequirements(renderer->device, mesh_data->ubo_buf, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_CPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &mesh_data->ubo_mem));

    OWL_VK_CHECK(vkMapMemory(renderer->device, mesh_data->ubo_mem, 0,
                             VK_WHOLE_SIZE, 0, &mesh_data->ubo_data));
  }

  {
    VkDescriptorSetAllocateInfo set;
    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = renderer->set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &renderer->node_set_layout;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set, &mesh_data->ubo_set));
  }

  {
    VkWriteDescriptorSet write;
    VkDescriptorBufferInfo buf;

    buf.buffer = mesh_data->ubo_buf;
    buf.offset = 0;
    buf.range = sizeof(struct owl_model_mesh_uniform_block);

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = mesh_data->ubo_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pImageInfo = NULL;
    write.pBufferInfo = &buf;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
  }

  for (i = 0; i < mesh_data->primitives_count; ++i)
    owl_process_primitive_(&mesh->primitives[i], model, state,
                           &mesh_data->primitives[i]);

end:
  return code;
}

OWL_INTERNAL void owl_deinit_mesh_(struct owl_vk_renderer const *renderer,
                                   struct owl_model_mesh_data const *data) {
  vkFreeDescriptorSets(renderer->device, renderer->set_pool, 1, &data->ubo_set);
  vkFreeMemory(renderer->device, data->ubo_mem, NULL);
  vkDestroyBuffer(renderer->device, data->ubo_buf, NULL);
}

OWL_INTERNAL enum owl_code
owl_process_node_(struct owl_vk_renderer const *renderer,
                  cgltf_node const *node, struct owl_model_node const *parent,
                  struct owl_model_load_state *state, struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model_node new_node;
  struct owl_model_node_data *new_node_data;

  if (OWL_SUCCESS != (code = owl_request_node_(model, &new_node)))
    goto end;

  new_node_data = &model->nodes[new_node.handle];

  if (node->mesh) {
    code = owl_process_mesh_(renderer, node->mesh, &new_node, state, model);

    if (OWL_SUCCESS != code)
      goto end;
  } else {
    new_node_data->mesh.handle = -1;
  }

  for (i = 0; i < (int)node->children_count; ++i) {
    code =
        owl_process_node_(renderer, node->children[i], &new_node, state, model);

    if (OWL_SUCCESS != code)
      goto end;
  }

  if (parent) {
    struct owl_model_node_data *parent_data = &model->nodes[parent->handle];
    int const child = parent_data->children_count++;

    if (OWL_MODEL_MAX_CHILDREN_PER_NODE <= parent_data->children_count) {
      code = OWL_ERROR_OUT_OF_SPACE;
      goto end;
    }

    parent_data->children[child].handle = new_node.handle;
  } else {
    int const new_root = model->roots_count++;

    if (OWL_MODEL_MAX_NODES <= model->roots_count) {
      code = OWL_ERROR_OUT_OF_SPACE;
      goto end;
    }

    model->roots[new_root].handle = new_node.handle;
  }

end:
  return code;
}

OWL_INTERNAL enum owl_sampler_addr_mode owl_get_wrap_mode_(int mode) {
  switch (mode) {
  case 10497:
    return OWL_SAMPLER_ADDR_MODE_REPEAT;

  case 33071:
    return OWL_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;

  case 33648:
    return OWL_SAMPLER_ADDR_MODE_MIRRORED_REPEAT;

  default:
    return OWL_SAMPLER_ADDR_MODE_REPEAT;
  }
}

OWL_INTERNAL enum owl_sampler_filter owl_get_filter_mode_(int type) {
  switch (type) {
  case 9728:
    return OWL_SAMPLER_FILTER_NEAREST;

  case 9729:
    return OWL_SAMPLER_FILTER_LINEAR;

  case 9984:
    return OWL_SAMPLER_FILTER_NEAREST;

  case 9985:
    return OWL_SAMPLER_FILTER_NEAREST;

  case 9986:
    return OWL_SAMPLER_FILTER_LINEAR;

  case 9987:
    return OWL_SAMPLER_FILTER_LINEAR;

  default:
    return OWL_SAMPLER_FILTER_LINEAR;
  }
}

enum owl_model_material_alpha_mode owl_get_alpha_mode_(cgltf_alpha_mode mode) {
  switch (mode) {
  case cgltf_alpha_mode_opaque:
    return OWL_MODEL_MATERIAL_ALPHA_MODE_OPAQUE;

  case cgltf_alpha_mode_mask:
    return OWL_MODEL_MATERIAL_ALPHA_MODE_MASK;

  case cgltf_alpha_mode_blend:
    return OWL_MODEL_MATERIAL_ALPHA_MODE_BLEND;
  }
}

char const *owl_fix_uri_(char const *uri) {
  OWL_LOCAL_PERSIST char buf[256];
  snprintf(buf, sizeof(buf), "../../assets/%s", uri);
  return buf;
}

enum owl_code owl_process_textures_(struct owl_vk_renderer *renderer,
                                    cgltf_data const *data,
                                    struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_MAX_TEXTURES <= data->textures_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  model->textures_count = (int)data->textures_count;

  for (i = 0; i < model->textures_count; ++i) {
    struct owl_texture_desc desc;
    cgltf_texture const *texture = &data->textures[i];
    struct owl_model_texture_data *texture_data = &model->textures[i];

    desc.mag_filter = owl_get_filter_mode_(texture->sampler->mag_filter);
    desc.min_filter = owl_get_filter_mode_(texture->sampler->min_filter);
    desc.wrap_u = owl_get_wrap_mode_(texture->sampler->wrap_s);
    desc.wrap_w = owl_get_wrap_mode_(texture->sampler->wrap_t);
    desc.wrap_v = owl_get_wrap_mode_(texture->sampler->wrap_t);

    code = owl_init_vk_texture_from_file(renderer, &desc,
                                         owl_fix_uri_(texture->image->uri),
                                         &texture_data->texture);

    texture_data->uri = texture->image->uri;

    if (OWL_SUCCESS != code)
      return OWL_ERROR_BAD_INIT;
  }

  return code;
}

OWL_INTERNAL enum owl_code owl_get_texture_(struct owl_model const *model,
                                            char const *uri,
                                            struct owl_model_texture *texture) {
  int i;

  for (i = 0; i < model->textures_count; ++i) {
    if (0 == strcmp(model->textures[i].uri, uri)) {
      texture->handle = i;
      return OWL_SUCCESS;
    }
  }

  return OWL_ERROR_UNKNOWN;
}

OWL_INTERNAL enum owl_code
owl_process_materials_(struct owl_vk_renderer *renderer, cgltf_data const *data,
                       struct owl_model *model) {
  int i;
  enum owl_code code;

  OWL_UNUSED(renderer);

  if (OWL_MODEL_MAX_MATERIALS <= data->materials_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  model->materials_count = (int)data->materials_count;

  for (i = 0; i < model->materials_count; ++i) {
    cgltf_material const *material = &data->materials[i];
    struct owl_model_material_data *material_data = &model->materials[i];

    if (material->pbr_metallic_roughness.base_color_texture.texture) {
      code = owl_get_texture_(model,
                              material->pbr_metallic_roughness
                                  .base_color_texture.texture->image->uri,
                              &material_data->base_color_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->base_color_texcoord =
          material->pbr_metallic_roughness.base_color_texture.texcoord;
    } else {
      material_data->base_color_texture.handle = -1;
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
      code =
          owl_get_texture_(model,
                           material->pbr_metallic_roughness
                               .metallic_roughness_texture.texture->image->uri,
                           &material_data->metallic_rougness_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->metallic_roughness_texcoord =
          material->pbr_metallic_roughness.metallic_roughness_texture.texcoord;
    } else {
      material_data->metallic_rougness_texture.handle = -1;
    }

    if (material->normal_texture.texture) {
      code =
          owl_get_texture_(model, material->normal_texture.texture->image->uri,
                           &material_data->metallic_rougness_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->normal_texcoord = material->normal_texture.texcoord;
    } else {
      material_data->normal_texture.handle = -1;
    }

    if (material->emissive_texture.texture) {
      code = owl_get_texture_(model,
                              material->emissive_texture.texture->image->uri,
                              &material_data->emissive_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->emissive_texcoord = material->emissive_texture.texcoord;
    } else {
      material_data->emissive_texture.handle = -1;
    }

    if (material->occlusion_texture.texture) {
      code = owl_get_texture_(model,
                              material->occlusion_texture.texture->image->uri,
                              &material_data->occlusion_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->occlusion_texcoord = material->occlusion_texture.texcoord;
    }

    material_data->alpha_mode = owl_get_alpha_mode_(material->alpha_mode);

    material_data->roughness_factor =
        material->pbr_metallic_roughness.roughness_factor;

    material_data->metallic_factor =
        material->pbr_metallic_roughness.metallic_factor;

    OWL_COPY_V4(material->pbr_metallic_roughness.base_color_factor,
                material_data->base_color_factor);

    material_data->alpha_cutoff = material->alpha_cutoff;

    OWL_SET_V4(0.0F, 0.0F, 0.0F, 1.0F, material_data->emissive_factor);
    OWL_COPY_V3(material->emissive_factor, material_data->emissive_factor);

    material_data->name = material->name;
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_add_vertices_and_indices_count_(cgltf_node const *node,
                                                      int *vertices_count,
                                                      int *indices_count) {
  int i;

  for (i = 0; i < (int)node->children_count; ++i)
    owl_add_vertices_and_indices_count_(node->children[i], vertices_count,
                                        indices_count);

  if (node->mesh) {
    for (i = 0; i < (int)node->mesh->primitives_count; ++i) {
      cgltf_primitive const *prim = &node->mesh->primitives[i];
      cgltf_attribute const *attr = owl_find_attr_(prim, "POSITION");

      *indices_count += prim->indices->count;
      *vertices_count += attr->data->count;
    }
  }
}

OWL_INTERNAL enum owl_code owl_submit_node_(struct owl_vk_renderer *renderer,
                                            struct owl_model const *model,
                                            struct owl_model_node const *node) {
  int i;
  int const active = renderer->dyn_active_buf;
  struct owl_model_node_data const *node_data = &model->nodes[node->handle];
  struct owl_model_mesh_data const *mesh_data =
      &model->meshes[node_data->mesh.handle];

  for (i = 0; i < mesh_data->primitives_count; ++i) {
    struct owl_model_primitive const *prim = &mesh_data->primitives[i];
    vkCmdDrawIndexed(renderer->frame_cmd_bufs[active], prim->indices_count, 1,
                     prim->first_index, 0, 0);
  }

  for (i = 0; i < node_data->children_count; ++i)
    owl_submit_node_(renderer, model, &node_data->children[i]);

  return OWL_SUCCESS;
}

enum owl_code owl_init_model_from_file(struct owl_vk_renderer *renderer,
                                       char const *path,
                                       struct owl_model *model) {
  cgltf_options options;
  cgltf_data *data = NULL;
  int i, vertices_count, indices_count;
  struct owl_model_load_state state;
  struct owl_dyn_buf_ref vertices_ref;
  struct owl_dyn_buf_ref indices_ref;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_is_dyn_buf_flushed(renderer));

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data))
    return OWL_ERROR_UNKNOWN;

  if (cgltf_result_success != (cgltf_load_buffers(&options, data, path)))
    return OWL_ERROR_UNKNOWN;

  if (OWL_SUCCESS != (code = owl_process_textures_(renderer, data, model)))
    goto end;

  if (OWL_SUCCESS != (code = owl_process_materials_(renderer, data, model)))
    goto end;

  vertices_count = 0;
  indices_count = 0;
  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_add_vertices_and_indices_count_(&data->nodes[i], &vertices_count,
                                        &indices_count);

  state.cur_vtx = 0;
  state.vtxs = owl_dyn_buf_alloc(
      renderer, (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex),
      &vertices_ref);

  state.cur_idx = 0;
  state.idxs = owl_dyn_buf_alloc(
      renderer, (VkDeviceSize)indices_count * sizeof(owl_u32), &indices_ref);

  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_process_node_(renderer, &data->nodes[i], NULL, &state, model);

  {
    VkBufferCreateInfo buf;
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.pNext = NULL;
    buf.flags = 0;
    buf.size = (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex);
    buf.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf.queueFamilyIndexCount = 0;
    buf.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buf, NULL, &model->vtx_buf));
  }

  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetBufferMemoryRequirements(renderer->device, model->vtx_buf, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_GPU_ONLY);
    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &model->vtx_mem));

    OWL_VK_CHECK(vkBindBufferMemory(renderer->device, model->vtx_buf,
                                    model->vtx_mem, 0));
  }

  {
    VkBufferCreateInfo buf;
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.pNext = NULL;
    buf.flags = 0;
    buf.size = (VkDeviceSize)indices_count * sizeof(owl_u32);
    buf.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf.queueFamilyIndexCount = 0;
    buf.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buf, NULL, &model->idx_buf));
  }

  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetBufferMemoryRequirements(renderer->device, model->idx_buf, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_GPU_ONLY);
    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &model->idx_mem));

    OWL_VK_CHECK(vkBindBufferMemory(renderer->device, model->idx_buf,
                                    model->idx_mem, 0));
  }

  {
    VkCommandBuffer cmd;
    owl_alloc_and_record_cmd_buf(renderer, &cmd);

    {
      VkBufferCopy copy;
      copy.srcOffset = vertices_ref.offset;
      copy.dstOffset = 0;
      copy.size =
          (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex);

      vkCmdCopyBuffer(cmd, vertices_ref.buf, model->vtx_buf, 1, &copy);
    }

    {
      VkBufferCopy copy;
      copy.srcOffset = indices_ref.offset;
      copy.dstOffset = 0;
      copy.size = (VkDeviceSize)indices_count * sizeof(owl_u32);

      vkCmdCopyBuffer(cmd, indices_ref.buf, model->idx_buf, 1, &copy);
    }

    owl_free_and_submit_cmd_buf(renderer, cmd);
  }

  owl_flush_dyn_buf(renderer);

  cgltf_free(data);

end:
  return code;
}

enum owl_code owl_create_model_from_file(struct owl_vk_renderer *renderer,
                                         char const *path,
                                         struct owl_model **model) {
  *model = OWL_MALLOC(sizeof(**model));
  return owl_init_model_from_file(renderer, path, *model);
}

void owl_deinit_model(struct owl_vk_renderer *renderer,
                      struct owl_model *model) {
  int i;
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  vkFreeMemory(renderer->device, model->idx_mem, NULL);
  vkDestroyBuffer(renderer->device, model->idx_buf, NULL);
  vkFreeMemory(renderer->device, model->vtx_mem, NULL);
  vkDestroyBuffer(renderer->device, model->vtx_buf, NULL);

  for (i = 0; i < model->textures_count; ++i)
    owl_deinit_vk_texture(renderer, &model->textures[i].texture);

  for (i = 0; i < model->meshes_count; ++i)
    owl_deinit_mesh_(renderer, &model->meshes[i]);
}

void owl_destroy_model(struct owl_vk_renderer *renderer,
                       struct owl_model *model) {
  owl_deinit_model(renderer, model);
  OWL_FREE(model);
}

enum owl_code owl_submit_model(struct owl_vk_renderer *renderer,
                               struct owl_draw_cmd_ubo const *ubo,
                               struct owl_model const *model) {
  int i;
  int const active = renderer->dyn_active_buf;

  {
    VkDeviceSize const offset = 0;
    vkCmdBindVertexBuffers(renderer->frame_cmd_bufs[active], 0, 1,
                           &model->vtx_buf, &offset);

    vkCmdBindIndexBuffer(renderer->frame_cmd_bufs[active], model->idx_buf, 0,
                         VK_INDEX_TYPE_UINT32);
  }

  {
    VkDeviceSize size;
    owl_byte *data;
    VkDescriptorSet sets[2];
    struct owl_dyn_buf_ref ref;

    size = sizeof(*ubo);
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = model->textures[1].texture.set;

    vkCmdBindDescriptorSets(
        renderer->frame_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderer->pipeline_layouts[renderer->bound_pipeline], 0,
        OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  for (i = 0; i < model->roots_count; ++i)
    owl_submit_node_(renderer, model, &model->roots[i]);

  return OWL_SUCCESS;
}

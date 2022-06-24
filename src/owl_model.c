#include "owl_model.h"

#include "cgltf.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_texture.h"
#include "owl_vector_math.h"

#include <float.h>
#include <stdio.h>

#if !defined(NDEBUG)

#define OWL_CHECK(e)                                                          \
  do {                                                                        \
    VkResult const result_ = e;                                               \
    if (VK_SUCCESS != result_)                                                \
      OWL_DEBUG_LOG("OWL_CHECK(%s) result = %i\n", #e, result_);              \
    OWL_ASSERT(VK_SUCCESS == result_);                                        \
  } while (0)

#else /* NDEBUG */

#define OWL_CHECK(e) e

#endif /* NDEBUG */

struct owl_model_uri {
  char path[OWL_MODEL_MAX_NAME_LENGTH];
};

static void const *
owl_resolve_gltf_accessor(struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  uint8_t const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

static owl_code
owl_model_uri_init(char const *src, struct owl_model_uri *uri) {
  owl_code code = OWL_OK;

  snprintf(uri->path, OWL_MODEL_MAX_NAME_LENGTH, "../../assets/%s", src);

  return code;
}

static owl_code
owl_model_images_load(struct owl_model *model, struct owl_renderer *renderer,
                      struct cgltf_data const *gltf) {
  int i;
  owl_code code = OWL_OK;

  for (i = 0; i < (int)gltf->images_count; ++i) {
    struct owl_model_uri uri;
    struct owl_texture_desc desc;
    struct owl_model_image *image = &model->images[i];

    code = owl_model_uri_init(gltf->images[i].uri, &uri);
    if (code) {
      goto out;
    }

    desc.source = OWL_TEXTURE_SOURCE_FILE;
    desc.type = OWL_TEXTURE_TYPE_2D;
    desc.path = uri.path;
    desc.width = 0;
    desc.height = 0;
    desc.format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;

    code = owl_texture_init(renderer, &desc, &image->image);
    if (code) {
      OWL_DEBUG_LOG("Failed to load texture %s!\n", desc.path);
      goto out;
    }
  }

  model->image_count = (int)gltf->images_count;

out:
  return code;
}

static owl_code
owl_model_textures_load(struct owl_model *model,
                        struct cgltf_data const *gltf) {
  int i;
  owl_code code = OWL_OK;

  model->texture_count = (int)gltf->textures_count;

  for (i = 0; i < (int)gltf->textures_count; ++i) {
    struct cgltf_texture const *gt = &gltf->textures[i];
    model->textures[i].image = (int)(gt->image - gltf->images);
  }

  return code;
}

static owl_code
owl_model_materials_load(struct owl_model *model,
                         struct cgltf_data const *gltf) {
  int i;
  owl_code code = OWL_OK;

  if (OWL_MODEL_MAX_NAME_LENGTH <= (int)gltf->materials_count) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  model->material_count = (int)gltf->materials_count;

  for (i = 0; i < (int)gltf->materials_count; ++i) {
    struct cgltf_material const *gm = &gltf->materials[i];
    struct owl_model_material *material = &model->materials[i];

    OWL_ASSERT(gm->has_pbr_metallic_roughness);

    OWL_V4_COPY(gm->pbr_metallic_roughness.base_color_factor,
                material->base_color_factor);

    material->base_color_texture =
        (owl_model_texture_id)(gm->pbr_metallic_roughness.base_color_texture
                                   .texture -
                               gltf->textures);

#if 0
    OWL_ASSERT(src_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (int)(src_material->normal_texture.texture - gltf->textures);
#else
    material->normal_texture = material->base_color_texture;
    material->physical_desc_texture = material->base_color_texture;
    material->occlusion_texture = OWL_MODEL_TEXTURE_NONE;
    material->emissive_texture = OWL_MODEL_TEXTURE_NONE;
#endif

    /* FIXME(samuel): make sure this is right */
    material->alpha_mode = (enum owl_model_alpha_mode)gm->alpha_mode;
    material->alpha_cutoff = gm->alpha_cutoff;
    material->double_sided = gm->double_sided;
  }

out:
  return code;
}

struct owl_model_load {
  int vertex_capacity;
  int vertex_count;
  struct owl_pnuujw_vertex *vertices;

  int index_capacity;
  int index_count;
  uint32_t *indices;
};

static struct cgltf_attribute const *
owl_find_gltf_attribute(struct cgltf_primitive const *p, char const *name) {
  int i;
  struct cgltf_attribute const *attr = NULL;

  for (i = 0; i < (int)p->attributes_count; ++i) {
    struct cgltf_attribute const *current = &p->attributes[i];

    if (!OWL_STRNCMP(current->name, name, OWL_MODEL_MAX_NAME_LENGTH)) {
      attr = current;
      goto out;
    }
  }

out:
  return attr;
}

static void
owl_model_load_find_capacities(struct owl_model_load *load,
                               struct cgltf_data const *gltf) {
  int i;
  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    int j;
    struct cgltf_node const *gn = &gltf->nodes[i];

    if (!gn->mesh) {
      continue;
    }

    for (j = 0; j < (int)gn->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attr;
      struct cgltf_primitive const *p = &gn->mesh->primitives[j];

      attr = owl_find_gltf_attribute(p, "POSITION");
      load->vertex_capacity += attr->data->count;
      load->index_capacity += p->indices->count;
    }
  }
}

static owl_code
owl_model_load_init(struct owl_model_load *load,
                    struct cgltf_data const *gltf) {
  uint64_t size;
  owl_code code = OWL_OK;

  load->vertex_capacity = 0;
  load->index_capacity = 0;
  load->vertex_count = 0;
  load->index_count = 0;

  owl_model_load_find_capacities(load, gltf);

  size = (uint64_t)load->vertex_capacity * sizeof(*load->vertices);
  load->vertices = OWL_MALLOC(size);
  if (!load->vertices) {
    code = OWL_ERROR_NO_MEMORY;
    goto out;
  }

  size = (uint64_t)load->index_capacity * sizeof(uint32_t);
  load->indices = OWL_MALLOC(size);
  if (!load->indices) {
    code = OWL_ERROR_NO_MEMORY;
    goto error_vertices_free;
  }

  goto out;

error_vertices_free:
  OWL_FREE(load->vertices);

out:
  return code;
}

static void
owl_model_load_deinit(struct owl_model_load *load) {
  OWL_FREE(load->indices);
  OWL_FREE(load->vertices);
}

static owl_code
owl_model_node_load(struct owl_model *model, struct cgltf_data const *gltf,
                    struct cgltf_node const *gn, struct owl_model_load *load) {
  int i;
  owl_model_node_id nid;
  struct owl_model_node *node;

  owl_code code = OWL_OK;

  nid = (owl_model_node_id)(gn - gltf->nodes);

  if (OWL_MODEL_MAX_ITEMS <= nid) {
    code = OWL_ERROR_NO_SPACE;
    goto out;
  }

  node = &model->nodes[nid];

  if (gn->parent) {
    node->parent = (owl_model_node_id)(gn->parent - gltf->nodes);
  } else {
    node->parent = OWL_MODEL_NODE_NONE;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gn->children_count) {
    code = OWL_ERROR_NO_SPACE;
    goto out;
  }

  node->child_count = (int)gn->children_count;

  for (i = 0; i < (int)gn->children_count; ++i) {
    node->children[i] = (owl_model_node_id)(gn->children[i] - gltf->nodes);
  }

  if (gn->name) {
    OWL_STRNCPY(node->name, gn->name, OWL_MODEL_MAX_NAME_LENGTH);
  } else {
    OWL_STRNCPY(node->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
  }

  if (gn->has_translation) {
    OWL_V3_COPY(gn->translation, node->translation);
  } else {
    OWL_V3_ZERO(node->translation);
  }

  if (gn->has_rotation) {
    OWL_V4_COPY(gn->rotation, node->rotation);
  } else {
    OWL_V4_ZERO(node->rotation);
  }

  if (gn->has_scale) {
    OWL_V3_COPY(gn->scale, node->scale);
  } else {
    OWL_V3_SET(node->scale, 1.0F, 1.0F, 1.0F);
  }

  if (gn->has_matrix) {
    OWL_M4_COPY_V16(gn->matrix, node->matrix);
  } else {
    OWL_V4_IDENTITY(node->matrix);
  }

  if (gn->skin) {
    node->skin = (owl_model_node_id)(gn->skin - gltf->skins);
  } else {
    node->skin = OWL_MODEL_SKIN_NONE;
  }

  if (gn->mesh) {
    struct cgltf_mesh const *gm;
    struct owl_model_mesh *md;

    node->mesh = model->mesh_count++;

    if (OWL_MODEL_MAX_ITEMS <= node->mesh) {
      code = OWL_ERROR_NO_SPACE;
      goto out;
    }

    gm = gn->mesh;
    md = &model->meshes[node->mesh];

    md->primitive_count = (int)gm->primitives_count;

    for (i = 0; i < (int)gm->primitives_count; ++i) {
      int j;
      int vertex_count = 0;
      int index_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv0 = NULL;
      float const *uv1 = NULL;
      uint16_t const *joints0 = NULL;
      float const *weights0 = NULL;
      struct cgltf_attribute const *attr = NULL;
      struct owl_model_primitive *pd = NULL;
      struct cgltf_primitive const *gp = &gm->primitives[i];

      md->primitives[i] = model->primitive_count++;

      if (OWL_MODEL_MAX_ITEMS <= md->primitives[i]) {
        code = OWL_ERROR_NO_SPACE;
        goto out;
      }

      pd = &model->primitives[md->primitives[i]];

      if ((attr = owl_find_gltf_attribute(gp, "POSITION"))) {
        position = owl_resolve_gltf_accessor(attr->data);
        vertex_count = (int)attr->data->count;
      }

      if ((attr = owl_find_gltf_attribute(gp, "NORMAL"))) {
        normal = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gp, "TEXCOORD_0"))) {
        uv0 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gp, "TEXCOORD_1"))) {
        uv1 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gp, "JOINTS_0"))) {
        joints0 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gp, "WEIGHTS_0"))) {
        weights0 = owl_resolve_gltf_accessor(attr->data);
      }

      for (j = 0; j < vertex_count; ++j) {
        int offset = load->vertex_count;
        struct owl_pnuujw_vertex *vertex = &load->vertices[offset + j];

        OWL_V3_COPY(&position[j * 3], vertex->position);

        if (normal) {
          OWL_V3_COPY(&normal[j * 3], vertex->normal);
        } else {
          OWL_V3_ZERO(vertex->normal);
        }

        if (uv0) {
          OWL_V2_COPY(&uv0[j * 2], vertex->uv0);
        } else {
          OWL_V2_ZERO(vertex->uv0);
        }

        if (uv1) {
          OWL_V2_COPY(&uv1[j * 2], vertex->uv1);
        } else {
          OWL_V2_ZERO(vertex->uv1);
        }

        if (joints0 && weights0) {
          OWL_V4_COPY(&joints0[j * 4], vertex->joints0);
        } else {
          OWL_V4_ZERO(vertex->joints0);
        }

        if (joints0 && weights0) {
          OWL_V4_COPY(&weights0[j * 4], vertex->weights0);
        } else {
          OWL_V4_ZERO(vertex->weights0);
        }
      }

      index_count = (int)gp->indices->count;

      switch (gp->indices->component_type) {
      case cgltf_component_type_r_32u: {
        uint32_t const *indices;
        int const offset = load->index_count;

        indices = owl_resolve_gltf_accessor(gp->indices);

        for (j = 0; j < (int)gp->indices->count; ++j) {
          load->indices[offset + j] = indices[j] +
                                      (uint32_t)load->vertex_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        uint16_t const *indices;
        int const offset = load->index_count;

        indices = owl_resolve_gltf_accessor(gp->indices);

        for (j = 0; j < (int)gp->indices->count; ++j) {
          load->indices[offset + j] = indices[j] +
                                      (uint16_t)load->vertex_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        uint8_t const *indices;
        int const offset = load->index_count;

        indices = owl_resolve_gltf_accessor(gp->indices);

        for (j = 0; j < (int)gp->indices->count; ++j) {
          load->indices[offset + j] = indices[j] + (uint8_t)load->vertex_count;
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_FATAL;
        goto out;
      }

      pd->first = (uint32_t)load->index_count;
      pd->count = (uint32_t)index_count;
      pd->material = (owl_model_material_id)(gp->material - gltf->materials);
      load->index_count += index_count;
      load->vertex_count += vertex_count;
    }
  }

out:
  return code;
}

static owl_code
owl_model_buffers_load(struct owl_model *model, struct owl_renderer *renderer,
                       struct owl_model_load const *load) {
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements memory_requirements;
  VkMemoryAllocateInfo memory_info;
  uint64_t size;
  uint8_t *data;
  VkBufferCopy copy;
  struct owl_renderer_upload_allocation allocation;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  OWL_ASSERT(load->vertex_count == load->vertex_capacity);
  OWL_ASSERT(load->index_count == load->index_capacity);

  size = (uint64_t)load->vertex_capacity * sizeof(struct owl_pnuujw_vertex);

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = 0;

  vk_result = vkCreateBuffer(renderer->device, &buffer_info, NULL,
                             &model->vk_vertex_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetBufferMemoryRequirements(renderer->device, model->vk_vertex_buffer,
                                &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
      renderer, memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                               &model->vk_vertex_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vk_result = vkBindBufferMemory(renderer->device, model->vk_vertex_buffer,
                                 model->vk_vertex_memory, 0);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  code = owl_renderer_begin_immediate_command_buffer(renderer);
  if (code)
    goto out;

  data = owl_renderer_upload_allocate(renderer, size, &allocation);
  if (!data) {
    code = OWL_ERROR_FATAL;
    goto out;
  }
  OWL_MEMCPY(data, load->vertices, size);

  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = size;

  vkCmdCopyBuffer(renderer->immediate_command_buffer, allocation.buffer,
                  model->vk_vertex_buffer, 1, &copy);

  code = owl_renderer_end_immediate_command_buffer(renderer);
  if (code)
    goto out;

  owl_renderer_upload_free(renderer, data);

  size = (uint64_t)load->index_capacity * sizeof(uint32_t);

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = 0;

  vk_result = vkCreateBuffer(renderer->device, &buffer_info, NULL,
                             &model->vk_index_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetBufferMemoryRequirements(renderer->device, model->vk_index_buffer,
                                &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
      renderer, memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                               &model->vk_index_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vk_result = vkBindBufferMemory(renderer->device, model->vk_index_buffer,
                                 model->vk_index_memory, 0);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  code = owl_renderer_begin_immediate_command_buffer(renderer);
  if (code)
    goto out;

  data = owl_renderer_upload_allocate(renderer, size, &allocation);
  if (!data) {
    code = OWL_ERROR_FATAL;
    goto out;
  }
  OWL_MEMCPY(data, load->indices, size);

  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = size;

  vkCmdCopyBuffer(renderer->immediate_command_buffer, allocation.buffer,
                  model->vk_index_buffer, 1, &copy);

  code = owl_renderer_end_immediate_command_buffer(renderer);
  if (code)
    goto out;

  owl_renderer_upload_free(renderer, data);

out:
  return code;
}

static owl_code
owl_model_nodes_load(struct owl_model *model, struct owl_renderer *renderer,
                     struct cgltf_data const *gltf) {
  int i;
  struct owl_model_load load;
  struct cgltf_scene const *gs;
  owl_code code = OWL_OK;

  gs = gltf->scene;

  code = owl_model_load_init(&load, gltf);
  if (code)
    goto out;

  for (i = 0; i < OWL_MODEL_MAX_ITEMS; ++i) {
    model->nodes[i].mesh = -1;
    model->nodes[i].parent = -1;
    model->nodes[i].mesh = -1;
    model->nodes[i].skin = -1;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->nodes_count) {
    code = OWL_ERROR_FATAL;
    goto error_deinit_load_state;
  }

  model->node_count = (int)gltf->nodes_count;

  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    code = owl_model_node_load(model, gltf, &gltf->nodes[i], &load);
    if (code)
      goto error_deinit_load_state;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gs->nodes_count) {
    code = OWL_ERROR_FATAL;
    goto error_deinit_load_state;
  }

  model->root_count = (int)gs->nodes_count;

  for (i = 0; i < (int)gs->nodes_count; ++i) {
    model->roots[i] = (owl_model_node_id)(gs->nodes[i] - gltf->nodes);
  }

  owl_model_buffers_load(model, renderer, &load);

error_deinit_load_state:
  owl_model_load_deinit(&load);

out:
  return code;
}

static owl_code
owl_model_skins_load(struct owl_model *model, struct owl_renderer *renderer,
                     struct cgltf_data const *gltf) {
  int i;
  owl_code code = OWL_OK;

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->skins_count) {
    code = OWL_ERROR_NO_SPACE;
    goto out;
  }

  model->skin_count = (int)gltf->skins_count;

  for (i = 0; i < (int)gltf->skins_count; ++i) {
    int j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *gs = &gltf->skins[i];
    struct owl_model_skin *skin = &model->skins[i];

    if (gs->name) {
      OWL_STRNCPY(skin->name, gs->name, OWL_MODEL_MAX_NAME_LENGTH);
    } else {
      OWL_STRNCPY(skin->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
    }

    skin->skeleton_root = (owl_model_node_id)(gs->skeleton - gltf->nodes);

    if (OWL_MODEL_MAX_JOINTS <= (int)gs->joints_count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_NO_SPACE;
      goto out;
    }

    skin->joint_count = (int)gs->joints_count;

    for (j = 0; j < (int)gs->joints_count; ++j) {
      skin->joints[j] = (owl_model_node_id)(gs->joints[j] - gltf->nodes);

      OWL_ASSERT(!OWL_STRNCMP(model->nodes[skin->joints[j]].name,
                              gs->joints[j]->name, OWL_MODEL_MAX_NAME_LENGTH));
    }

    inverse_bind_matrices = owl_resolve_gltf_accessor(
        gs->inverse_bind_matrices);

    for (j = 0; j < (int)gs->inverse_bind_matrices->count; ++j) {
      OWL_M4_COPY(inverse_bind_matrices[j], skin->inverse_bind_matrices[j]);
    }

    {
      VkBufferCreateInfo info;

      skin->ssbo_buffer_size = sizeof(struct owl_model_skin_ssbo);

      info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.size = skin->ssbo_buffer_size;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        OWL_CHECK(vkCreateBuffer(renderer->device, &info, NULL,
                                 &skin->ssbo_buffers[j]));
      }
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo info;

      vkGetBufferMemoryRequirements(renderer->device, skin->ssbo_buffers[0],
                                    &requirements);

      skin->ssbo_buffer_alignment = requirements.alignment;
      skin->ssbo_buffer_aligned_size = OWL_ALIGN_UP_2(skin->ssbo_buffer_size,
                                                      requirements.alignment);

      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = NULL;
      info.allocationSize = skin->ssbo_buffer_aligned_size *
                            OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
      info.memoryTypeIndex = owl_renderer_find_memory_type(
          renderer, requirements.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      OWL_CHECK(
          vkAllocateMemory(renderer->device, &info, NULL, &skin->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        OWL_CHECK(vkBindBufferMemory(
            renderer->device, skin->ssbo_buffers[j], skin->ssbo_memory,
            (uint64_t)j * skin->ssbo_buffer_aligned_size));
      }
    }

    {
      VkDescriptorSetLayout layouts[OWL_MODEL_MAX_ITEMS];
      VkDescriptorSetAllocateInfo info;

      for (j = 0; j < (int)renderer->frame_count; ++j)
        layouts[j] = renderer->ssbo_vertex_descriptor_set_layout;

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = renderer->descriptor_pool;
      info.descriptorSetCount = renderer->frame_count;
      info.pSetLayouts = layouts;

      OWL_CHECK(
          vkAllocateDescriptorSets(renderer->device, &info, skin->ssbo_sets));
    }

    {
      for (j = 0; j < (int)renderer->frame_count; ++j) {
        VkDescriptorBufferInfo info;
        VkWriteDescriptorSet write;

        info.buffer = skin->ssbo_buffers[j];
        info.offset = 0;
        info.range = skin->ssbo_buffer_size;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = NULL;
        write.dstSet = skin->ssbo_sets[j];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pImageInfo = NULL;
        write.pBufferInfo = &info;
        write.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
      }
    }

    {
      int k;
      void *data;

      vkMapMemory(renderer->device, skin->ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data);

      for (j = 0; j < (int)renderer->frame_count; ++j) {
        uint64_t offset = (uint64_t)j * skin->ssbo_buffer_aligned_size;
        uint8_t *ssbo = &((uint8_t *)data)[offset];
        skin->ssbos[j] = (struct owl_model_skin_ssbo *)ssbo;
      }

      for (j = 0; j < (int)renderer->frame_count; ++j) {
        struct owl_model_skin_ssbo *ssbo = skin->ssbos[j];

        OWL_V4_IDENTITY(ssbo->matrix);

        for (k = 0; k < (int)gs->inverse_bind_matrices->count; ++k) {
          OWL_M4_COPY(inverse_bind_matrices[k], ssbo->joint_matices[k]);
        }

        ssbo->joint_matrix_count = skin->joint_count;
      }
    }
  }
out:
  return code;
}

static owl_code
owl_model_anims_load(struct cgltf_data const *gltf, struct owl_model *model) {
  int i;
  owl_code code = OWL_OK;

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->animations_count) {
    code = OWL_ERROR_NO_SPACE;
    goto out;
  }

  model->anim_count = (int)gltf->animations_count;

  for (i = 0; i < (int)gltf->animations_count; ++i) {
    int j;
    struct cgltf_animation const *ga;
    struct owl_model_anim *anim;

    ga = &gltf->animations[i];
    anim = &model->anims[i];

    anim->current_time = 0.0F;

    if (OWL_MODEL_MAX_ITEMS <= (int)ga->samplers_count) {
      code = OWL_ERROR_NO_SPACE;
      goto out;
    }

    anim->sampler_count = (int)ga->samplers_count;

    anim->begin = FLT_MAX;
    anim->end = FLT_MIN;

    for (j = 0; j < (int)ga->samplers_count; ++j) {
      int k;
      float const *inputs;
      struct cgltf_animation_sampler const *gs;
      owl_model_anim_sampler_id sid;
      struct owl_model_anim_sampler *sampler;

      gs = &ga->samplers[j];

      sid = model->anim_sampler_count++;

      if (OWL_MODEL_MAX_ITEMS <= sid) {
        code = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler = &model->anim_samplers[sid];

      if (OWL_MODEL_MAX_ITEMS <= (int)gs->input->count) {
        code = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler->input_count = (int)gs->input->count;

      inputs = owl_resolve_gltf_accessor(gs->input);

      for (k = 0; k < (int)gs->input->count; ++k) {
        float const input = inputs[k];

        sampler->inputs[k] = input;

        if (input < anim->begin) {
          anim->begin = input;
        }

        if (input > anim->end) {
          anim->end = input;
        }
      }

      if (OWL_MODEL_MAX_ITEMS <= (int)gs->output->count) {
        code = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler->output_count = (int)gs->output->count;

      switch (gs->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor(gs->output);

        for (k = 0; k < (int)gs->output->count; ++k) {
          OWL_V4_ZERO(sampler->outputs[k]);
          OWL_V3_COPY(outputs[k], sampler->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor(gs->output);
        for (k = 0; k < (int)gs->output->count; ++k) {
          OWL_V4_COPY(outputs[k], sampler->outputs[k]);
        }
      } break;

      case cgltf_type_invalid:
      case cgltf_type_scalar:
      case cgltf_type_vec2:
      case cgltf_type_mat2:
      case cgltf_type_mat3:
      case cgltf_type_mat4:
      default:
        code = OWL_ERROR_FATAL;
        goto out;
      }

      sampler->interpolation = (int)gs->interpolation;

      anim->samplers[j] = sid;
    }

    if (OWL_MODEL_MAX_ITEMS <= (int)ga->channels_count) {
      code = OWL_ERROR_NO_SPACE;
      goto out;
    }

    anim->chan_count = (int)ga->channels_count;

    for (j = 0; j < (int)ga->channels_count; ++j) {
      struct cgltf_animation_channel const *gc;
      owl_model_anim_chan_id cid;
      struct owl_model_anim_chan *chan;

      gc = &ga->channels[j];

      cid = model->anim_chan_count++;

      if (OWL_MODEL_MAX_ITEMS <= cid) {
        code = OWL_ERROR_NO_SPACE;
        goto out;
      }

      chan = &model->anim_chans[cid];

      chan->path = (int)gc->target_path;
      chan->node = (owl_model_anim_chan_id)(gc->target_node - gltf->nodes);
      chan->anim_sampler = anim->samplers[(gc->sampler - ga->samplers)];

      anim->chans[j] = cid;
    }
  }

out:
  return code;
}

OWL_PUBLIC owl_code
owl_model_init(struct owl_model *model, struct owl_renderer *renderer,
               char const *path) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;

  owl_code code = OWL_OK;

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));

  model->root_count = 0;
  model->node_count = 0;
  model->image_count = 0;
  model->texture_count = 0;
  model->material_count = 0;
  model->mesh_count = 0;
  model->primitive_count = 0;
  model->skin_count = 0;
  model->anim_sampler_count = 0;
  model->anim_chan_count = 0;
  model->anim_count = 0;
  model->active_anim = 0;

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data)) {
    OWL_DEBUG_LOG("Filed to parse gltf file!");
    code = OWL_ERROR_FATAL;
    goto out;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    OWL_DEBUG_LOG("Filed to parse load gltf buffers!");
    code = OWL_ERROR_FATAL;
    goto error_data_free;
  }

  code = owl_model_images_load(model, renderer, data);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf images!");
    goto error_data_free;
  }

  code = owl_model_textures_load(model, data);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf textures!");
    goto error_data_free;
  }

  code = owl_model_materials_load(model, data);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf materials!");
    goto error_data_free;
  }

  code = owl_model_nodes_load(model, renderer, data);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf nodes!");
    goto error_data_free;
  }

  code = owl_model_skins_load(model, renderer, data);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf skins!");
    goto error_data_free;
  }

  code = owl_model_anims_load(data, model);
  if (code) {
    OWL_DEBUG_LOG("Filed to parse load gltf animations!");
    goto error_data_free;
  }

error_data_free:
  cgltf_free(data);

out:
  return code;
}

OWL_PUBLIC void
owl_model_deinit(struct owl_model *model, struct owl_renderer *renderer) {
  int i;

  vkDeviceWaitIdle(renderer->device);

  vkFreeMemory(renderer->device, model->vk_index_memory, NULL);
  vkDestroyBuffer(renderer->device, model->vk_index_buffer, NULL);
  vkFreeMemory(renderer->device, model->vk_vertex_memory, NULL);
  vkDestroyBuffer(renderer->device, model->vk_vertex_buffer, NULL);

  for (i = 0; i < model->image_count; ++i) {
    owl_texture_deinit(renderer, &model->images[i].image);
  }

  for (i = 0; i < model->skin_count; ++i) {
    int j;

    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool,
                         renderer->frame_count, model->skins[i].ssbo_sets);

    vkFreeMemory(renderer->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < (int)renderer->frame_count; ++j) {
      vkDestroyBuffer(renderer->device, model->skins[i].ssbo_buffers[j], NULL);
    }
  }
}

static void
owl_model_resolve_local_node_matrix(struct owl_model const *model,
                                    owl_model_node_id nid, owl_m4 matrix) {
  owl_m4 tmp;
  struct owl_model_node const *node;

  node = &model->nodes[nid];

  OWL_V4_IDENTITY(matrix);
  owl_m4_translate(node->translation, matrix);

  OWL_V4_IDENTITY(tmp);
  owl_q4_as_m4(node->rotation, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  OWL_V4_IDENTITY(tmp);
  owl_m4_scale_v3(tmp, node->scale, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  owl_m4_multiply(matrix, node->matrix, matrix);
}

static void
owl_model_resolve_node_matrix(struct owl_model const *model,
                              owl_model_node_id nid, owl_m4 matrix) {
  owl_model_node_id parent;

  owl_model_resolve_local_node_matrix(model, nid, matrix);

  for (parent = model->nodes[nid].parent; OWL_MODEL_NODE_NONE != parent;
       parent = model->nodes[parent].parent) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix(model, parent, local);
    owl_m4_multiply(local, matrix, matrix);
  }
}

static void
owl_model_node_joints_update(struct owl_model *model, int frame,
                             owl_model_node_id nid) {
  int i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin const *skin;
  struct owl_model_node const *node;

  node = &model->nodes[nid];

  for (i = 0; i < node->child_count; ++i) {
    owl_model_node_joints_update(model, frame, node->children[i]);
  }

  if (OWL_MODEL_SKIN_NONE == node->skin) {
    goto out;
  }

  skin = &model->skins[node->skin];

  owl_model_resolve_node_matrix(model, nid, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < skin->joint_count; ++i) {
    struct owl_model_skin_ssbo *ssbo = skin->ssbos[frame];

    owl_model_resolve_node_matrix(model, skin->joints[i], tmp);
    owl_m4_multiply(tmp, skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp, ssbo->joint_matices[i]);
  }

out:
  return;
}

#define OWL_MODEL_ANIM_INTERPOLATION_TYPE_LINEAR                              \
  cgltf_interpolation_type_linear
#define OWL_MODEL_ANIM_PATH_TYPE_TRANSLATION                                  \
  cgltf_animation_path_type_translation
#define OWL_MODEL_ANIM_PATH_TYPE_ROTATION cgltf_animation_path_type_rotation
#define OWL_MODEL_ANIM_PATH_TYPE_SCALE cgltf_animation_path_type_scale

OWL_PUBLIC owl_code
owl_model_anim_update(struct owl_model *model, int frame, float dt,
                      owl_model_anim_id id) {
  int i;
  struct owl_model_anim *anim;

  owl_code code = OWL_OK;

  if (OWL_MODEL_ANIM_NONE == id) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  anim = &model->anims[id];

  if (anim->end < (anim->current_time += dt)) {
    anim->current_time -= anim->end;
  }

  for (i = 0; i < anim->chan_count; ++i) {
    int j;
    owl_model_anim_chan_id cid;
    owl_model_anim_sampler_id sid;
    struct owl_model_node *node;
    struct owl_model_anim_chan const *chan;
    struct owl_model_anim_sampler const *sampler;

    cid = anim->chans[i];
    chan = &model->anim_chans[cid];

    sid = chan->anim_sampler;
    sampler = &model->anim_samplers[sid];

    node = &model->nodes[chan->node];

    if (OWL_MODEL_ANIM_INTERPOLATION_TYPE_LINEAR != sampler->interpolation) {
      continue;
    }

    for (j = 0; j < sampler->input_count - 1; ++j) {
      float const i0 = sampler->inputs[j];
      float const i1 = sampler->inputs[j + 1];
      float const ct = anim->current_time;
      float const a = (ct - i0) / (i1 - i0);

      if (!((ct >= i0) && (ct <= i1))) {
        continue;
      }

      switch (chan->path) {
      case OWL_MODEL_ANIM_PATH_TYPE_TRANSLATION: {
        owl_v3_mix(sampler->outputs[j], sampler->outputs[j + 1], a,
                   node->translation);
      } break;

      case OWL_MODEL_ANIM_PATH_TYPE_ROTATION: {
        owl_v4_quat_slerp(sampler->outputs[j], sampler->outputs[j + 1], a,
                          node->rotation);
        owl_v4_normalize(node->rotation, node->rotation);
      } break;

      case OWL_MODEL_ANIM_PATH_TYPE_SCALE: {
        owl_v3_mix(sampler->outputs[j], sampler->outputs[j + 1], a,
                   node->scale);
      } break;

      default:
        OWL_ASSERT(0 && "unexpected path");
        code = OWL_ERROR_FATAL;
        goto out;
      }
    }
  }

  for (i = 0; i < model->root_count; ++i) {
    owl_model_node_joints_update(model, frame, model->roots[i]);
  }

out:
  return code;
}

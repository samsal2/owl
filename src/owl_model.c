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

static int owl_model_uri_init(char const *src, struct owl_model_uri *uri) {
  int ret = OWL_OK;

  snprintf(uri->path, OWL_MODEL_MAX_NAME_LENGTH, "../../assets/%s", src);

  return ret;
}

static int owl_model_images_load(struct owl_model *model,
                                 struct owl_renderer *r,
                                 struct cgltf_data const *gltf) {
  int i;
  int ret = OWL_OK;

  for (i = 0; i < (int)gltf->images_count; ++i) {
    struct owl_model_uri uri;
    struct owl_texture_desc desc;
    struct owl_model_image *image = &model->images[i];

    ret = owl_model_uri_init(gltf->images[i].uri, &uri);
    if (ret) {
      goto out;
    }

    desc.source = OWL_TEXTURE_SOURCE_FILE;
    desc.type = OWL_TEXTURE_TYPE_2D;
    desc.path = uri.path;
    desc.width = 0;
    desc.height = 0;
    desc.format = OWL_RGBA8_SRGB;

    ret = owl_texture_init(r, &desc, &image->image);
    if (ret) {
      OWL_DEBUG_LOG("Failed to load texture %s!\n", desc.path);
      goto out;
    }
  }

  model->num_images = (int)gltf->images_count;

out:
  return ret;
}

static int owl_model_textures_load(struct owl_model *model,
                                   struct cgltf_data const *gltf) {
  int i;
  int ret = OWL_OK;

  model->num_textures = (int)gltf->textures_count;

  for (i = 0; i < (int)gltf->textures_count; ++i) {
    struct cgltf_texture const *gt = &gltf->textures[i];
    model->textures[i].image = (int)(gt->image - gltf->images);
  }

  return ret;
}

static int owl_model_materials_load(struct owl_model *model,
                                    struct owl_renderer *r,
                                    struct cgltf_data const *gltf) {
  int i;
  int ret = OWL_OK;

  if (OWL_MODEL_MAX_NAME_LENGTH <= (int)gltf->materials_count) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  model->num_materials = (int)gltf->materials_count;

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

    {
      VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
      VkResult vk_result;

      descriptor_set_allocate_info.sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptor_set_allocate_info.pNext = NULL;
      descriptor_set_allocate_info.descriptorPool = r->descriptor_pool;
      descriptor_set_allocate_info.descriptorSetCount = 1;
      descriptor_set_allocate_info.pSetLayouts =
          &r->model_material_descriptor_set_layout;

      vk_result = vkAllocateDescriptorSets(
          r->device, &descriptor_set_allocate_info, &material->descriptor_set);
      if (vk_result) {
        ret = OWL_ERROR_FATAL;
        goto out;
      }
    }

    {
      VkDescriptorImageInfo descriptors[4];
      VkWriteDescriptorSet writes[4];

      descriptors[0].sampler = r->linear_sampler;
      descriptors[0].imageView = NULL;
      descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[1].sampler = VK_NULL_HANDLE;
      descriptors[1].imageView =
          model->images[model->textures[material->base_color_texture].image]
              .image.image_view;
      descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[2].sampler = VK_NULL_HANDLE;
      descriptors[2].imageView =
          model->images[model->textures[material->normal_texture].image]
              .image.image_view;
      descriptors[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[3].sampler = VK_NULL_HANDLE;
      descriptors[3].imageView =
          model->images[model->textures[material->physical_desc_texture].image]
              .image.image_view;
      descriptors[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[0].pNext = NULL;
      writes[0].dstSet = material->descriptor_set;
      writes[0].dstBinding = 0;
      writes[0].dstArrayElement = 0;
      writes[0].descriptorCount = 1;
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
      writes[0].pImageInfo = &descriptors[0];
      writes[0].pBufferInfo = NULL;
      writes[0].pTexelBufferView = NULL;

      writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[1].pNext = NULL;
      writes[1].dstSet = material->descriptor_set;
      writes[1].dstBinding = 1;
      writes[1].dstArrayElement = 0;
      writes[1].descriptorCount = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      writes[1].pImageInfo = &descriptors[1];
      writes[1].pBufferInfo = NULL;
      writes[1].pTexelBufferView = NULL;

      writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[2].pNext = NULL;
      writes[2].dstSet = material->descriptor_set;
      writes[2].dstBinding = 2;
      writes[2].dstArrayElement = 0;
      writes[2].descriptorCount = 1;
      writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      writes[2].pImageInfo = &descriptors[2];
      writes[2].pBufferInfo = NULL;
      writes[2].pTexelBufferView = NULL;

      writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[3].pNext = NULL;
      writes[3].dstSet = material->descriptor_set;
      writes[3].dstBinding = 3;
      writes[3].dstArrayElement = 0;
      writes[3].descriptorCount = 1;
      writes[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      writes[3].pImageInfo = &descriptors[3];
      writes[3].pBufferInfo = NULL;
      writes[3].pTexelBufferView = NULL;

      vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0,
                             NULL);
    }
  }

out:
  return ret;
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

static void owl_model_load_find_capacities(struct owl_model_load *load,
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

static int owl_model_load_init(struct owl_model_load *load,
                               struct cgltf_data const *gltf) {
  uint64_t size;
  int ret = OWL_OK;

  load->vertex_capacity = 0;
  load->index_capacity = 0;
  load->vertex_count = 0;
  load->index_count = 0;

  owl_model_load_find_capacities(load, gltf);

  size = (uint64_t)load->vertex_capacity * sizeof(*load->vertices);
  load->vertices = OWL_MALLOC(size);
  if (!load->vertices) {
    ret = OWL_ERROR_NO_MEMORY;
    goto out;
  }

  size = (uint64_t)load->index_capacity * sizeof(uint32_t);
  load->indices = OWL_MALLOC(size);
  if (!load->indices) {
    ret = OWL_ERROR_NO_MEMORY;
    goto error_vertices_free;
  }

  goto out;

error_vertices_free:
  OWL_FREE(load->vertices);

out:
  return ret;
}

static void owl_model_load_deinit(struct owl_model_load *load) {
  OWL_FREE(load->indices);
  OWL_FREE(load->vertices);
}

static int owl_model_node_load(struct owl_model *model,
                               struct cgltf_data const *gltf,
                               struct cgltf_node const *gn,
                               struct owl_model_load *load) {
  int i;
  owl_model_node_id nid;
  struct owl_model_node *node;

  int ret = OWL_OK;

  nid = (owl_model_node_id)(gn - gltf->nodes);

  if (OWL_MODEL_MAX_ITEMS <= nid) {
    ret = OWL_ERROR_NO_SPACE;
    goto out;
  }

  node = &model->nodes[nid];

  if (gn->parent) {
    node->parent = (owl_model_node_id)(gn->parent - gltf->nodes);
  } else {
    node->parent = OWL_MODEL_NODE_NONE;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gn->children_count) {
    ret = OWL_ERROR_NO_SPACE;
    goto out;
  }

  node->num_children = (int)gn->children_count;

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

    node->mesh = model->num_meshes++;

    if (OWL_MODEL_MAX_ITEMS <= node->mesh) {
      ret = OWL_ERROR_NO_SPACE;
      goto out;
    }

    gm = gn->mesh;
    md = &model->meshes[node->mesh];

    md->num_primitives = (int)gm->primitives_count;

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

      md->primitives[i] = model->num_primitives++;

      if (OWL_MODEL_MAX_ITEMS <= md->primitives[i]) {
        ret = OWL_ERROR_NO_SPACE;
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
        ret = OWL_ERROR_FATAL;
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
  return ret;
}

static int owl_model_buffers_load(struct owl_model *model,
                                  struct owl_renderer *r,
                                  struct owl_model_load const *load) {
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements memory_requirements;
  VkMemoryAllocateInfo memory_info;
  uint64_t size;
  uint8_t *data;
  VkBufferCopy copy;
  struct owl_renderer_upload_allocation allocation;

  VkResult vk_result = VK_SUCCESS;
  int ret = OWL_OK;

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

  vk_result = vkCreateBuffer(r->device, &buffer_info, NULL,
                             &model->vertex_buffer);
  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetBufferMemoryRequirements(r->device, model->vertex_buffer,
                                &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
      r, memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(r->device, &memory_info, NULL,
                               &model->vertex_memory);
  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  vk_result = vkBindBufferMemory(r->device, model->vertex_buffer,
                                 model->vertex_memory, 0);

  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  ret = owl_renderer_begin_im_command_buffer(r);
  if (ret)
    goto out;

  data = owl_renderer_upload_allocate(r, size, &allocation);
  if (!data) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }
  OWL_MEMCPY(data, load->vertices, size);

  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = size;

  vkCmdCopyBuffer(r->im_command_buffer, allocation.buffer,
                  model->vertex_buffer, 1, &copy);

  ret = owl_renderer_end_im_command_buffer(r);
  if (ret)
    goto out;

  owl_renderer_upload_free(r, data);

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

  vk_result = vkCreateBuffer(r->device, &buffer_info, NULL,
                             &model->index_buffer);
  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetBufferMemoryRequirements(r->device, model->index_buffer,
                                &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
      r, memory_requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(r->device, &memory_info, NULL,
                               &model->index_memory);
  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  vk_result = vkBindBufferMemory(r->device, model->index_buffer,
                                 model->index_memory, 0);
  if (VK_SUCCESS != vk_result) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  ret = owl_renderer_begin_im_command_buffer(r);
  if (ret)
    goto out;

  data = owl_renderer_upload_allocate(r, size, &allocation);
  if (!data) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }
  OWL_MEMCPY(data, load->indices, size);

  copy.srcOffset = 0;
  copy.dstOffset = 0;
  copy.size = size;

  vkCmdCopyBuffer(r->im_command_buffer, allocation.buffer, model->index_buffer,
                  1, &copy);

  ret = owl_renderer_end_im_command_buffer(r);
  if (ret)
    goto out;

  owl_renderer_upload_free(r, data);

out:
  return ret;
}

static int owl_model_nodes_load(struct owl_model *model,
                                struct owl_renderer *r,
                                struct cgltf_data const *gltf) {
  int i;
  struct owl_model_load load;
  struct cgltf_scene const *gs;
  int ret = OWL_OK;

  gs = gltf->scene;

  ret = owl_model_load_init(&load, gltf);
  if (ret)
    goto out;

  for (i = 0; i < OWL_MODEL_MAX_ITEMS; ++i) {
    model->nodes[i].mesh = -1;
    model->nodes[i].parent = -1;
    model->nodes[i].mesh = -1;
    model->nodes[i].skin = -1;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->nodes_count) {
    ret = OWL_ERROR_FATAL;
    goto error_deinit_load_state;
  }

  model->num_nodes = (int)gltf->nodes_count;

  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    ret = owl_model_node_load(model, gltf, &gltf->nodes[i], &load);
    if (ret)
      goto error_deinit_load_state;
  }

  if (OWL_MODEL_MAX_ITEMS <= (int)gs->nodes_count) {
    ret = OWL_ERROR_FATAL;
    goto error_deinit_load_state;
  }

  model->num_roots = (int)gs->nodes_count;

  for (i = 0; i < (int)gs->nodes_count; ++i) {
    model->roots[i] = (owl_model_node_id)(gs->nodes[i] - gltf->nodes);
  }

  owl_model_buffers_load(model, r, &load);

error_deinit_load_state:
  owl_model_load_deinit(&load);

out:
  return ret;
}

static int owl_model_skins_load(struct owl_model *model,
                                struct owl_renderer *r,
                                struct cgltf_data const *gltf) {
  int i;
  int ret = OWL_OK;

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->skins_count) {
    ret = OWL_ERROR_NO_SPACE;
    goto out;
  }

  model->num_skins = (int)gltf->skins_count;

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
      ret = OWL_ERROR_NO_SPACE;
      goto out;
    }

    skin->num_joints = (int)gs->joints_count;

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

      for (j = 0; j < OWL_NUM_IN_FLIGHT_FRAMES; ++j) {
        OWL_CHECK(
            vkCreateBuffer(r->device, &info, NULL, &skin->ssbo_buffers[j]));
      }
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo info;

      vkGetBufferMemoryRequirements(r->device, skin->ssbo_buffers[0],
                                    &requirements);

      skin->ssbo_buffer_alignment = requirements.alignment;
      skin->ssbo_buffer_aligned_size = OWL_ALIGN_UP_2(skin->ssbo_buffer_size,
                                                      requirements.alignment);

      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = NULL;
      info.allocationSize = skin->ssbo_buffer_aligned_size *
                            OWL_NUM_IN_FLIGHT_FRAMES;
      info.memoryTypeIndex = owl_renderer_find_memory_type(
          r, requirements.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      OWL_CHECK(vkAllocateMemory(r->device, &info, NULL, &skin->ssbo_memory));

      for (j = 0; j < OWL_NUM_IN_FLIGHT_FRAMES; ++j) {
        OWL_CHECK(vkBindBufferMemory(
            r->device, skin->ssbo_buffers[j], skin->ssbo_memory,
            (uint64_t)j * skin->ssbo_buffer_aligned_size));
      }
    }

    {
      VkDescriptorSetLayout layouts[OWL_MODEL_MAX_ITEMS];
      VkDescriptorSetAllocateInfo info;

      for (j = 0; j < (int)r->num_frames; ++j)
        layouts[j] = r->model_joints_descriptor_set_layout;

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = r->descriptor_pool;
      info.descriptorSetCount = r->num_frames;
      info.pSetLayouts = layouts;

      OWL_CHECK(vkAllocateDescriptorSets(r->device, &info, skin->ssbo_sets));
    }

    {
      for (j = 0; j < (int)r->num_frames; ++j) {
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

        vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
      }
    }

    {
      int k;
      void *data;

      vkMapMemory(r->device, skin->ssbo_memory, 0, VK_WHOLE_SIZE, 0, &data);

      for (j = 0; j < (int)r->num_frames; ++j) {
        uint64_t offset = (uint64_t)j * skin->ssbo_buffer_aligned_size;
        uint8_t *ssbo = &((uint8_t *)data)[offset];
        skin->ssbos[j] = (struct owl_model_skin_ssbo *)ssbo;
      }

      for (j = 0; j < (int)r->num_frames; ++j) {
        struct owl_model_skin_ssbo *ssbo = skin->ssbos[j];

        OWL_V4_IDENTITY(ssbo->matrix);

        for (k = 0; k < (int)gs->inverse_bind_matrices->count; ++k) {
          OWL_M4_COPY(inverse_bind_matrices[k], ssbo->joint_matices[k]);
        }

        ssbo->num_joint_matrices = skin->num_joints;
      }
    }
  }
out:
  return ret;
}

static int owl_model_anims_load(struct cgltf_data const *gltf,
                                struct owl_model *model) {
  int i;
  int ret = OWL_OK;

  if (OWL_MODEL_MAX_ITEMS <= (int)gltf->animations_count) {
    ret = OWL_ERROR_NO_SPACE;
    goto out;
  }

  model->num_anim = (int)gltf->animations_count;

  for (i = 0; i < (int)gltf->animations_count; ++i) {
    int j;
    struct cgltf_animation const *ga;
    struct owl_model_anim *anim;

    ga = &gltf->animations[i];
    anim = &model->anims[i];

    anim->current_time = 0.0F;

    if (OWL_MODEL_MAX_ITEMS <= (int)ga->samplers_count) {
      ret = OWL_ERROR_NO_SPACE;
      goto out;
    }

    anim->num_samplers = (int)ga->samplers_count;

    anim->begin = FLT_MAX;
    anim->end = FLT_MIN;

    for (j = 0; j < (int)ga->samplers_count; ++j) {
      int k;
      float const *inputs;
      struct cgltf_animation_sampler const *gs;
      owl_model_anim_sampler_id sid;
      struct owl_model_anim_sampler *sampler;

      gs = &ga->samplers[j];

      sid = model->num_anim_samplers++;

      if (OWL_MODEL_MAX_ITEMS <= sid) {
        ret = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler = &model->anim_samplers[sid];

      if (OWL_MODEL_MAX_ITEMS <= (int)gs->input->count) {
        ret = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler->num_inputs = (int)gs->input->count;

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
        ret = OWL_ERROR_NO_SPACE;
        goto out;
      }

      sampler->num_outputs = (int)gs->output->count;

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
        ret = OWL_ERROR_FATAL;
        goto out;
      }

      sampler->interpolation = (int)gs->interpolation;

      anim->samplers[j] = sid;
    }

    if (OWL_MODEL_MAX_ITEMS <= (int)ga->channels_count) {
      ret = OWL_ERROR_NO_SPACE;
      goto out;
    }

    anim->num_chans = (int)ga->channels_count;

    for (j = 0; j < (int)ga->channels_count; ++j) {
      struct cgltf_animation_channel const *gc;
      owl_model_anim_chan_id cid;
      struct owl_model_anim_chan *chan;

      gc = &ga->channels[j];

      cid = model->num_anim_chans++;

      if (OWL_MODEL_MAX_ITEMS <= cid) {
        ret = OWL_ERROR_NO_SPACE;
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
  return ret;
}

OWLAPI int owl_model_init(struct owl_model *model, struct owl_renderer *r,
                          char const *path) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;

  int ret = OWL_OK;

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));

  model->num_roots = 0;
  model->num_nodes = 0;
  model->num_images = 0;
  model->num_textures = 0;
  model->num_materials = 0;
  model->num_meshes = 0;
  model->num_primitives = 0;
  model->num_skins = 0;
  model->num_anim_samplers = 0;
  model->num_anim_chans = 0;
  model->num_anim = 0;
  model->active_anim = 0;

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data)) {
    OWL_DEBUG_LOG("Filed to parse gltf file!");
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    OWL_DEBUG_LOG("Filed to parse load gltf buffers!");
    ret = OWL_ERROR_FATAL;
    goto error_data_free;
  }

  ret = owl_model_images_load(model, r, data);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf images!");
    goto error_data_free;
  }

  ret = owl_model_textures_load(model, data);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf textures!");
    goto error_data_free;
  }

  ret = owl_model_materials_load(model, r, data);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf materials!");
    goto error_data_free;
  }

  ret = owl_model_nodes_load(model, r, data);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf nodes!");
    goto error_data_free;
  }

  ret = owl_model_skins_load(model, r, data);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf skins!");
    goto error_data_free;
  }

  ret = owl_model_anims_load(data, model);
  if (ret) {
    OWL_DEBUG_LOG("Filed to parse load gltf animations!");
    goto error_data_free;
  }

error_data_free:
  cgltf_free(data);

out:
  return ret;
}

OWLAPI void owl_model_deinit(struct owl_model *model, struct owl_renderer *r) {
  int i;

  vkDeviceWaitIdle(r->device);

  vkFreeMemory(r->device, model->index_memory, NULL);
  vkDestroyBuffer(r->device, model->index_buffer, NULL);
  vkFreeMemory(r->device, model->vertex_memory, NULL);
  vkDestroyBuffer(r->device, model->vertex_buffer, NULL);

  for (i = 0; i < model->num_images; ++i) {
    owl_texture_deinit(r, &model->images[i].image);
  }

  for (i = 0; i < model->num_skins; ++i) {
    int j;

    vkFreeDescriptorSets(r->device, r->descriptor_pool, r->num_frames,
                         model->skins[i].ssbo_sets);

    vkFreeMemory(r->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < (int)r->num_frames; ++j) {
      vkDestroyBuffer(r->device, model->skins[i].ssbo_buffers[j], NULL);
    }
  }

  for (i = 0; i < model->num_materials; ++i) {
    vkFreeDescriptorSets(r->device, r->descriptor_pool, 1,
                         &model->materials[i].descriptor_set);
  }
}

static void owl_model_resolve_local_node_matrix(struct owl_model const *model,
                                                owl_model_node_id nid,
                                                owl_m4 matrix) {
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

static void owl_model_resolve_node_matrix(struct owl_model const *model,
                                          owl_model_node_id nid,
                                          owl_m4 matrix) {
  owl_model_node_id parent;

  owl_model_resolve_local_node_matrix(model, nid, matrix);

  for (parent = model->nodes[nid].parent; OWL_MODEL_NODE_NONE != parent;
       parent = model->nodes[parent].parent) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix(model, parent, local);
    owl_m4_multiply(local, matrix, matrix);
  }
}

static void owl_model_node_joints_update(struct owl_model *model, int frame,
                                         owl_model_node_id nid) {
  int i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin const *skin;
  struct owl_model_node const *node;

  node = &model->nodes[nid];

  for (i = 0; i < node->num_children; ++i) {
    owl_model_node_joints_update(model, frame, node->children[i]);
  }

  if (OWL_MODEL_SKIN_NONE == node->skin) {
    goto out;
  }

  skin = &model->skins[node->skin];

  owl_model_resolve_node_matrix(model, nid, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < skin->num_joints; ++i) {
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

OWLAPI int owl_model_anim_update(struct owl_model *model, int frame, float dt,
                                 owl_model_anim_id id) {
  int i;
  struct owl_model_anim *anim;

  int ret = OWL_OK;

  if (OWL_MODEL_ANIM_NONE == id) {
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  anim = &model->anims[id];

  if (anim->end < (anim->current_time += dt)) {
    anim->current_time -= anim->end;
  }

  for (i = 0; i < anim->num_chans; ++i) {
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

    for (j = 0; j < sampler->num_inputs - 1; ++j) {
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
        ret = OWL_ERROR_FATAL;
        goto out;
      }
    }
  }

  for (i = 0; i < model->num_roots; ++i) {
    owl_model_node_joints_update(model, frame, model->roots[i]);
  }

out:
  return ret;
}

#include "model.h"

#include "internal.h"
#include "renderer.h"
#include "texture.h"
#include "vector_math.h"

#include <cgltf/cgltf.h>
#include <math.h>
#include <stb/stb_image.h>
#include <stdio.h>

#define OWL_MODEL_MESH_HANDLE_NONE -1
#define OWL_MODEL_TEXTURE_HANDLE_NONE -1

struct owl_model_load_state {
  int current_index;
  owl_u32 *indices;

  int current_vertex;
  struct owl_model_vertex *vertices;
};

OWL_INTERNAL enum owl_code
owl_model_request_node_(struct owl_model *model, struct owl_model_node *node) {
  int const handle = model->nodes_count++;

  if (OWL_MODEL_MAX_NODES <= model->nodes_count)
    return OWL_ERROR_OUT_OF_SPACE;

  node->handle = handle;

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code
owl_model_request_mesh_(struct owl_model *model, struct owl_model_mesh *mesh) {
  int const handle = model->meshes_count++;

  if (OWL_MODEL_MAX_MESHES <= model->meshes_count)
    return OWL_ERROR_OUT_OF_SPACE;

  mesh->handle = handle;

  return OWL_SUCCESS;
}

OWL_INTERNAL cgltf_attribute const *
owl_gltf_find_attr_(cgltf_primitive const *prim, char const *name) {
  int i;

  for (i = 0; i < (int)prim->attributes_count; ++i)
    if (0 == strcmp(prim->attributes[i].name, name))
      return &prim->attributes[i];

  return NULL;
}

OWL_INTERNAL void const *owl_gltf_get_attr_data_(cgltf_attribute const *attr) {
  owl_byte const *bdata = attr->data->buffer_view->buffer->data;
  return &bdata[attr->data->buffer_view->offset + attr->data->offset];
}

OWL_INTERNAL int owl_gltf_get_type_size_(cgltf_type type) {
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
owl_model_get_material_(struct owl_model const *model, char const *name,
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

OWL_INTERNAL enum owl_code owl_model_process_primitive_(
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

  out->first_index = (owl_u32)state->current_index;
  out->indices_count = (owl_u32)prim->indices->count;

  /* position */
  if ((attr = owl_gltf_find_attr_(prim, "POSITION"))) {
    pos_data = owl_gltf_get_attr_data_(attr);
    pos_stride = owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
    out->vertex_count = (owl_u32)attr->data->count;
  } else {
    return OWL_ERROR_UNKNOWN;
  }

  if ((attr = owl_gltf_find_attr_(prim, "NORMAL"))) {
    normal_data = owl_gltf_get_attr_data_(attr);
    normal_stride =
        owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_gltf_find_attr_(prim, "TEXCOORD_0"))) {
    uv0_data = owl_gltf_get_attr_data_(attr);
    uv0_stride = owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_gltf_find_attr_(prim, "TEXCOORD_1"))) {
    uv1_data = owl_gltf_get_attr_data_(attr);
    uv1_stride = owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  if ((attr = owl_gltf_find_attr_(prim, "JOINTS_0"))) {
    joint_data = owl_gltf_get_attr_data_(attr);
    joint_stride =
        owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
    joint_type = attr->data->component_type;
  }

  if ((attr = owl_gltf_find_attr_(prim, "WEIGHTS_0"))) {
    weight_data = owl_gltf_get_attr_data_(attr);
    weight_stride =
        owl_gltf_get_type_size_(attr->data->type) * (int)sizeof(float);
  }

  /* vertices */
  for (i = 0; i < (int)out->vertex_count; ++i) {
    struct owl_model_vertex *v = &state->vertices[state->current_vertex + i];
    {
      float const *data = (float const *)&pos_data[i * pos_stride];
      v->position[0] = data[0];
      v->position[1] = -data[1]; /* FIXME(samuel): y coords are reversed */
      v->position[2] = data[2];
    }

    if (normal_data) {
      float const *data = (float const *)&normal_data[i * normal_stride];
      v->normal[0] = data[0];
      v->normal[1] = data[1];
      v->normal[2] = data[2];
    } else {
      OWL_V3_ZERO(v->normal);
    }

    if (uv0_data) {
      float const *data = (float const *)&uv0_data[i * uv0_stride];
      v->uv0[0] = data[0];
      v->uv0[1] = data[1];
    } else {
      OWL_V2_ZERO(v->uv0);
    }

    if (uv1_data) {
      float const *data = (float const *)&uv1_data[i * uv1_stride];
      v->uv1[0] = data[0];
      v->uv1[1] = data[1];
    } else {
      OWL_V2_ZERO(v->uv1);
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
      OWL_V4_ZERO(v->joint0);
      OWL_V4_ZERO(v->weight0);
    }
  }

  state->current_vertex += (int)out->vertex_count;

  /* indices */
  {
    owl_byte const *idx_data = &(
        (owl_byte const *)prim->indices->buffer_view->buffer
            ->data)[prim->indices->buffer_view->offset + prim->indices->offset];

    switch (prim->indices->component_type) {
    case cgltf_component_type_r_8u: { /* UNSIGNED_BYTE */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->indices[state->current_index + i] =
            ((owl_u8 const *)idx_data)[i];
      }
    } break;
    case cgltf_component_type_r_16u: { /* UNSIGNED_SHORT */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->indices[state->current_index + i] =
            ((owl_u16 const *)idx_data)[i];
      }
    } break;
    case cgltf_component_type_r_32u: { /* UNSIGNED_INT */
      for (i = 0; i < (int)prim->indices->count; ++i) {
        state->indices[state->current_index + i] =
            ((owl_u32 const *)idx_data)[i];
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

  state->current_index += (int)prim->indices->count;

  code = owl_model_get_material_(model, prim->material->name, &out->material);

  if (OWL_SUCCESS != code)
    return code;

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_process_mesh_(struct owl_renderer const *r, cgltf_mesh const *mesh,
                        struct owl_model_node const *node,
                        struct owl_model_load_state *state,
                        struct owl_model *model) {
  int i;
  struct owl_model_mesh_data *mesh_data;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model_node_data *node_data = &model->nodes[node->handle];

  code = owl_model_request_mesh_(model, &node_data->mesh);

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
    buffer.size = sizeof(struct owl_model_mesh_ubo);
    buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &mesh_data->ubo_buffer));
  }

  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetBufferMemoryRequirements(r->device, mesh_data->ubo_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_CPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &mesh_data->ubo_memory));

    OWL_VK_CHECK(vkMapMemory(r->device, mesh_data->ubo_memory, 0, VK_WHOLE_SIZE,
                             0, &mesh_data->ubo_data));
  }

  {
    VkDescriptorSetAllocateInfo set;
    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = r->common_set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &r->node_set_layout;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(r->device, &set, &mesh_data->ubo_set));
  }

  {
    VkWriteDescriptorSet write;
    VkDescriptorBufferInfo buffer;

    buffer.buffer = mesh_data->ubo_buffer;
    buffer.offset = 0;
    buffer.range = sizeof(struct owl_model_mesh_ubo);

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = mesh_data->ubo_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pImageInfo = NULL;
    write.pBufferInfo = &buffer;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
  }

  for (i = 0; i < mesh_data->primitives_count; ++i)
    owl_model_process_primitive_(&mesh->primitives[i], model, state,
                                 &mesh_data->primitives[i]);

end:
  return code;
}

OWL_INTERNAL void
owl_model_deinit_mesh_(struct owl_renderer const *r,
                       struct owl_model_mesh_data const *data) {
  vkFreeDescriptorSets(r->device, r->common_set_pool, 1, &data->ubo_set);
  vkFreeMemory(r->device, data->ubo_memory, NULL);
  vkDestroyBuffer(r->device, data->ubo_buffer, NULL);
}

OWL_INTERNAL enum owl_code
owl_model_process_node_(struct owl_renderer const *r, cgltf_node const *node,
                        struct owl_model_node const *parent,
                        struct owl_model_load_state *state,
                        struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;
  struct owl_model_node new_node;
  struct owl_model_node_data *new_node_data;

  if (OWL_SUCCESS != (code = owl_model_request_node_(model, &new_node)))
    goto end;

  new_node_data = &model->nodes[new_node.handle];

  if (node->mesh) {
    code = owl_model_process_mesh_(r, node->mesh, &new_node, state, model);

    if (OWL_SUCCESS != code)
      goto end;
  } else {
    new_node_data->mesh.handle = OWL_MODEL_MESH_HANDLE_NONE;
  }

  for (i = 0; i < (int)node->children_count; ++i) {
    code =
        owl_model_process_node_(r, node->children[i], &new_node, state, model);

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

OWL_INTERNAL VkSamplerAddressMode owl_as_vk_sampler_addr_mode_(int mode) {
  switch (mode) {
  case 10497:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;

  case 33071:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  case 33648:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

  default:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  }
}

OWL_INTERNAL VkFilter owl_as_vk_filter_(int type) {
  switch (type) {
  case 9728:
    return VK_FILTER_NEAREST;

  case 9729:
    return VK_FILTER_LINEAR;

  case 9984:
    return VK_FILTER_NEAREST;

  case 9985:
    return VK_FILTER_NEAREST;

  case 9986:
    return VK_FILTER_LINEAR;

  case 9987:
    return VK_FILTER_LINEAR;

  default:
    return VK_FILTER_LINEAR;
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
  OWL_LOCAL_PERSIST char buffer[256];
  snprintf(buffer, sizeof(buffer), "../../assets/%s", uri);
  return buffer;
}

struct owl_model_texture_sampler_info {
  int mag_filter;
  int min_filter;
  int wrap_u;
  int wrap_v;
};

OWL_INTERNAL enum owl_code owl_model_init_texture_data_(
    struct owl_renderer *r, char const *path,
    struct owl_model_texture_sampler_info const *sampler_info,
    struct owl_model_texture_data *tex) {
  owl_byte *data;
  struct owl_texture_init_info info;
  enum owl_code code = OWL_SUCCESS;

  strncpy(tex->uri, path, sizeof(tex->uri));

  if (!(data = owl_texture_data_from_file(path, &info)))
    return OWL_ERROR_BAD_ALLOC;

  tex->mips = owl_calc_mips((owl_u32)info.width, (owl_u32)info.height);

  {
#define OWL_IMAGE_USAGE                                                        \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |          \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_as_vk_format_(info.format);
    image.extent.width = (owl_u32)info.width;
    image.extent.height = (owl_u32)info.height;
    image.extent.depth = 1;
    image.mipLevels = (owl_u32)tex->mips;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.usage = OWL_IMAGE_USAGE;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(r->device, &image, NULL, &tex->image));

#undef OWL_IMAGE_USAGE
  }

  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetImageMemoryRequirements(r->device, tex->image, &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(vkAllocateMemory(r->device, &memory, NULL, &tex->memory));
    OWL_VK_CHECK(vkBindImageMemory(r->device, tex->image, tex->memory, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = tex->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_as_vk_format_(info.format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = tex->mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(r->device, &view, NULL, &tex->view));
  }

  {
    VkCommandBuffer command;
    VkDeviceSize size;
    owl_byte *stage;
    struct owl_dynamic_buffer_reference ref;

    size = owl_texture_init_info_required_size_(&info);

    if (!(stage = owl_renderer_dynamic_buffer_alloc(r, size, &ref)))
      return OWL_ERROR_BAD_ALLOC;

    owl_renderer_alloc_single_use_command_buffer(r, &command);

    {
      struct owl_vk_image_transition_info transition;

      transition.mips = tex->mips;
      transition.layers = 1;
      transition.from = VK_IMAGE_LAYOUT_UNDEFINED;
      transition.to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      transition.image = tex->image;

      owl_vk_image_transition(command, &transition);
    }

    {

      VkBufferImageCopy copy;

      copy.bufferOffset = ref.offset;
      copy.bufferRowLength = 0;
      copy.bufferImageHeight = 0;
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.mipLevel = 0;
      copy.imageSubresource.baseArrayLayer = 0;
      copy.imageSubresource.layerCount = 1;
      copy.imageOffset.x = 0;
      copy.imageOffset.y = 0;
      copy.imageOffset.z = 0;
      copy.imageExtent.width = (owl_u32)info.width;
      copy.imageExtent.height = (owl_u32)info.height;
      copy.imageExtent.depth = 1;

      vkCmdCopyBufferToImage(command, ref.buffer, tex->image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }
    {
      struct owl_vk_image_mip_info mip;

      mip.width = info.width;
      mip.height = info.height;
      mip.mips = tex->mips;
      mip.image = tex->image;

      owl_vk_image_generate_mips(command, &mip);
    }

    owl_renderer_free_single_use_command_buffer(r, command);
  }

  {
    /* FIXME(samuel): correct mipmap mode */
#define OWL_MAX_ANISOTROPY 16

    VkSamplerCreateInfo sampler;
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext = NULL;
    sampler.flags = 0;
    sampler.magFilter = owl_as_vk_filter_(sampler_info->mag_filter);
    sampler.minFilter = owl_as_vk_filter_(sampler_info->min_filter);
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = owl_as_vk_sampler_addr_mode_(sampler_info->wrap_u);
    sampler.addressModeV = owl_as_vk_sampler_addr_mode_(sampler_info->wrap_v);
    sampler.addressModeW = owl_as_vk_sampler_addr_mode_(sampler_info->wrap_v);
    sampler.mipLodBias = 0.0F;
    sampler.anisotropyEnable = VK_TRUE;
    sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
    sampler.compareEnable = VK_FALSE;
    sampler.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler.minLod = 0.0F;
    sampler.maxLod = tex->mips; /* VK_LOD_CLAMP_NONE */
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;

    OWL_VK_CHECK(vkCreateSampler(r->device, &sampler, NULL, &tex->sampler));

#undef OWL_MAX_ANISOTROPY
  }

  owl_texture_free_data_from_file(data);

  return code;
}

OWL_INTERNAL void
owl_model_deinit_texture_data_(struct owl_renderer *r,
                               struct owl_model_texture_data *tex) {
  vkDestroyImage(r->device, tex->image, NULL);
  vkFreeMemory(r->device, tex->memory, NULL);
  vkDestroyImageView(r->device, tex->view, NULL);
  vkDestroySampler(r->device, tex->sampler, NULL);
}

enum owl_code owl_model_process_textures_(struct owl_renderer *r,
                                          cgltf_data const *data,
                                          struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_MAX_TEXTURES <= data->textures_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  model->textures_count = (int)data->textures_count;

  for (i = 0; i < model->textures_count; ++i) {
    struct owl_model_texture_sampler_info sampler;
    cgltf_texture const *texture = &data->textures[i];
    struct owl_model_texture_data *texture_data = &model->textures[i];

    sampler.mag_filter = texture->sampler->mag_filter;
    sampler.min_filter = texture->sampler->min_filter;
    sampler.wrap_u = texture->sampler->wrap_s;
    sampler.wrap_v = texture->sampler->wrap_t;

    owl_model_init_texture_data_(r, texture_data->uri, &sampler, texture_data);
  }

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_get_texture_(struct owl_model const *model, char const *uri,
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
owl_model_process_materials_(struct owl_renderer *r, cgltf_data const *data,
                             struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_MATERIALS <= data->materials_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  model->materials_count = (int)data->materials_count;

  for (i = 0; i < model->materials_count; ++i) {
    cgltf_material const *material = &data->materials[i];
    struct owl_model_material_data *material_data = &model->materials[i];

    if (material->pbr_metallic_roughness.base_color_texture.texture) {
      code = owl_model_get_texture_(model,
                                    material->pbr_metallic_roughness
                                        .base_color_texture.texture->image->uri,
                                    &material_data->base_color_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->base_color_texcoord =
          material->pbr_metallic_roughness.base_color_texture.texcoord;
    } else {
      material_data->base_color_texture.handle = OWL_MODEL_TEXTURE_HANDLE_NONE;
    }

    if (material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
      code = owl_model_get_texture_(
          model,
          material->pbr_metallic_roughness.metallic_roughness_texture.texture
              ->image->uri,
          &material_data->metallic_roughness_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->metallic_roughness_texcoord =
          material->pbr_metallic_roughness.metallic_roughness_texture.texcoord;
    } else {
      material_data->metallic_roughness_texture.handle =
          OWL_MODEL_TEXTURE_HANDLE_NONE;
    }

    if (material->normal_texture.texture) {
      code = owl_model_get_texture_(
          model, material->normal_texture.texture->image->uri,
          &material_data->metallic_roughness_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->normal_texcoord = material->normal_texture.texcoord;
    } else {
      material_data->normal_texture.handle = OWL_MODEL_TEXTURE_HANDLE_NONE;
    }

    if (material->emissive_texture.texture) {
      code = owl_model_get_texture_(
          model, material->emissive_texture.texture->image->uri,
          &material_data->emissive_texture);

      if (OWL_SUCCESS != code)
        return code;

      material_data->emissive_texcoord = material->emissive_texture.texcoord;
    } else {
      material_data->emissive_texture.handle = OWL_MODEL_TEXTURE_HANDLE_NONE;
    }

    if (material->occlusion_texture.texture) {
      code = owl_model_get_texture_(
          model, material->occlusion_texture.texture->image->uri,
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

    OWL_V4_COPY(material->pbr_metallic_roughness.base_color_factor,
                material_data->base_color_factor);

    material_data->alpha_cutoff = material->alpha_cutoff;

    OWL_V4_SET(0.0F, 0.0F, 0.0F, 1.0F, material_data->emissive_factor);
    OWL_V3_COPY(material->emissive_factor, material_data->emissive_factor);

    material_data->name = material->name;

#if 0
    {
      VkDescriptorSetAllocateInfo set;
      VkDescriptorSetLayout layouts[1];

      layouts[0] = r->material_set_layout;

      set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      set.pNext = NULL;
      set.descriptorPool = r->common_set_pool;
      set.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
      set.pSetLayouts = layouts;

      OWL_VK_CHECK(
          vkAllocateDescriptorSets(r->device, &set, &material_data->set));
    }
#endif

#if 0
    {
      int handle;
      owl_u32 j;
      VkWriteDescriptorSet writes[5];
      VkDescriptorImageInfo images[5];

      /* FIXME(samuel): check for invalid textures */
      handle = material_data->base_color_texture.handle;
      images[0].sampler = model->textures[handle].sampler;
      images[0].imageView = model->textures[handle].view;
      images[0].imageLayout = model->textures[handle].image_layout;

      handle = material_data->metallic_roughness_texture.handle;
      images[1].sampler = model->textures[handle].sampler;
      images[1].imageView = model->textures[handle].view;
      images[1].imageLayout = model->textures[handle].image_layout;

      handle = material_data->normal_texture.handle;
      images[2].sampler = model->textures[handle].sampler;
      images[2].imageView = model->textures[handle].view;
      images[2].imageLayout = model->textures[handle].image_layout;

      handle = material_data->occlusion_texture.handle;
      images[3].sampler = model->textures[handle].sampler;
      images[3].imageView = model->textures[handle].view;
      images[3].imageLayout = model->textures[handle].image_layout;

      handle = material_data->emissive_texture.handle;
      images[4].sampler = model->textures[handle].sampler;
      images[4].imageView = model->textures[handle].view;
      images[4].imageLayout = model->textures[handle].image_layout;

      // FIXME: not using combined image sampler atm
      OWL_ASSERT(0 && "FATAL");
      for (j = 0; j < OWL_ARRAY_SIZE(writes); ++j) {
        writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[j].pNext = NULL;
        writes[j].dstSet = material_data->set;
        writes[j].dstBinding = j;
        writes[j].dstArrayElement = 0;
        writes[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[j].pImageInfo = &images[j];
        writes[j].pBufferInfo = NULL;
        writes[j].pTexelBufferView = NULL;
      }

      vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0,
                             NULL);
    }
#endif
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
      cgltf_attribute const *attr = owl_gltf_find_attr_(prim, "POSITION");

      *indices_count += prim->indices->count;
      *vertices_count += attr->data->count;
    }
  }
}

OWL_INTERNAL enum owl_code owl_submit_node_(struct owl_renderer *r,
                                            struct owl_model const *model,
                                            struct owl_model_node const *node) {
  int i;
  struct owl_model_node_data const *data;
  struct owl_model_mesh_data const *mesh;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_MESH_HANDLE_NONE == model->nodes[node->handle].mesh.handle)
    goto end;

  data = &model->nodes[node->handle];
  mesh = &model->meshes[data->mesh.handle];

  for (i = 0; i < mesh->primitives_count; ++i) {
    VkDescriptorSet sets[3];
    struct owl_material_push_constant push_constant;
    struct owl_model_primitive const *primitive;
    struct owl_model_material_data const *material;

    primitive = &mesh->primitives[i];
    material = &model->materials[primitive->material.handle];

#if 0
    sets[0] = model->scene.set;
#endif
    sets[1] = model->materials[primitive->material.handle].set;
    sets[2] = mesh->ubo_set;

    vkCmdBindDescriptorSets(r->active_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 0, NULL);

    OWL_V4_ZERO(push_constant.base_color_factor);

    OWL_V4_COPY(material->emissive_factor, push_constant.emissive_factor);

    OWL_V4_ZERO(push_constant.diffuse_factor);
    OWL_V4_ZERO(push_constant.specular_factor);

    push_constant.workflow = 0;

    push_constant.base_color_texture_set = material->base_color_texcoord;
    push_constant.physical_descriptor_texture_set = 0;
    push_constant.normal_texture_set = material->normal_texcoord;
    push_constant.occlusion_texture_set = material->occlusion_texcoord;
    push_constant.emissive_texture_set = material->emissive_texcoord;
    push_constant.metallic_factor = material->metallic_factor;
    push_constant.roughness_factor = material->roughness_factor;
    push_constant.alpha_mask = 1;
    push_constant.alpha_mask_cutoff = material->alpha_cutoff;

    vkCmdPushConstants(
        r->active_command_buffer, r->pipeline_layouts[r->bound_pipeline],
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push_constant), &push_constant);

    if (primitive->has_indices) 
      vkCmdDrawIndexed(r->active_command_buffer, primitive->indices_count,
                       1, primitive->first_index, 0, 0);
    else
      vkCmdDraw(r->active_command_buffer, primitive->vertex_count, 1, 0, 0);
  }

  for (i = 0; i < data->children_count; ++i)
    if (OWL_SUCCESS == (code = owl_submit_node_(r, model, &data->children[i])))
      goto end;

end:
  return code;
}

enum owl_code
owl_model_alloc_buffers_(struct owl_renderer *r, int vertices_count,
                         int indices_count,
                         struct owl_dynamic_buffer_reference const *vertices,
                         struct owl_dynamic_buffer_reference const *indices,
                         struct owl_model *model) {
  {
    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size =
        (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex);
    buffer.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &model->vertex_buffer));
  }

  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetBufferMemoryRequirements(r->device, model->vertex_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &model->vertex_memory));

    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->vertex_buffer,
                                    model->vertex_memory, 0));
  }

  {
    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size = (VkDeviceSize)indices_count * sizeof(owl_u32);
    buffer.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = NULL;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &model->index_buffer));
  }

  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetBufferMemoryRequirements(r->device, model->index_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &model->index_memory));

    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->index_buffer,
                                    model->index_memory, 0));
  }

  {
    VkCommandBuffer command;
    owl_renderer_alloc_single_use_command_buffer(r, &command);

    {
      VkBufferCopy copy;
      copy.srcOffset = vertices->offset;
      copy.dstOffset = 0;
      copy.size =
          (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex);

      vkCmdCopyBuffer(command, vertices->buffer, model->vertex_buffer, 1,
                      &copy);
    }

    {
      VkBufferCopy copy;
      copy.srcOffset = indices->offset;
      copy.dstOffset = 0;
      copy.size = (VkDeviceSize)indices_count * sizeof(owl_u32);

      vkCmdCopyBuffer(command, indices->buffer, model->index_buffer, 1, &copy);
    }

    owl_renderer_free_single_use_command_buffer(r, command);
  }

  return OWL_SUCCESS;
}

enum owl_code owl_model_init_from_file(struct owl_renderer *r, char const *path,
                                       struct owl_model *model) {
  cgltf_options options;
  cgltf_data *data = NULL;
  int i;
  struct owl_model_load_state state;
  struct owl_dynamic_buffer_reference vertices_ref;
  struct owl_dynamic_buffer_reference indices_ref;
  int vertices_count = 0;
  int indices_count = 0;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_buffer_clear(r));

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data))
    return OWL_ERROR_UNKNOWN;

  if (cgltf_result_success != (cgltf_load_buffers(&options, data, path)))
    return OWL_ERROR_UNKNOWN;

  if (OWL_SUCCESS != (code = owl_model_process_textures_(r, data, model)))
    goto end;

  if (OWL_SUCCESS != (code = owl_model_process_materials_(r, data, model)))
    goto end;

  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_add_vertices_and_indices_count_(&data->nodes[i], &vertices_count,
                                        &indices_count);

  state.current_vertex = 0;
  state.vertices = owl_renderer_dynamic_buffer_alloc(
      r, (VkDeviceSize)vertices_count * sizeof(struct owl_model_vertex),
      &vertices_ref);

  state.current_index = 0;
  state.indices = owl_renderer_dynamic_buffer_alloc(
      r, (VkDeviceSize)indices_count * sizeof(owl_u32), &indices_ref);

  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_model_process_node_(r, &data->nodes[i], NULL, &state, model);

  code = owl_model_alloc_buffers_(r, vertices_count, indices_count,
                                  &vertices_ref, &indices_ref, model);

  if (OWL_SUCCESS != code)
    goto end;

  owl_renderer_clear_dynamic_offset(r);

  cgltf_free(data);

end:
  return code;
}

enum owl_code owl_model_create_from_file(struct owl_renderer *r,
                                         char const *path,
                                         struct owl_model **model) {
  *model = OWL_MALLOC(sizeof(**model));
  return owl_model_init_from_file(r, path, *model);
}

void owl_model_deinit(struct owl_renderer *r, struct owl_model *model) {
  int i;
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeMemory(r->device, model->index_memory, NULL);
  vkDestroyBuffer(r->device, model->index_buffer, NULL);
  vkFreeMemory(r->device, model->vertex_memory, NULL);
  vkDestroyBuffer(r->device, model->vertex_buffer, NULL);

  for (i = 0; i < model->textures_count; ++i)
    owl_model_deinit_texture_data_(r, &model->textures[i]);

  for (i = 0; i < model->meshes_count; ++i)
    owl_model_deinit_mesh_(r, &model->meshes[i]);
}

void owl_model_destroy(struct owl_renderer *r, struct owl_model *model) {
  owl_model_deinit(r, model);
  OWL_FREE(model);
}

OWL_INTERNAL void
owl_model_bind_vertex_and_index_buffers_(struct owl_renderer *r,
                                         struct owl_model const *model) {
  VkDeviceSize const offset[] = {0};
  vkCmdBindVertexBuffers(r->active_command_buffer, 0, 1, &model->vertex_buffer,
                         offset);

  vkCmdBindIndexBuffer(r->active_command_buffer, model->index_buffer, 0,
                       VK_INDEX_TYPE_UINT32);
}

#if 0

OWL_INTERNAL void
owl_model_submit_primitives_(struct owl_renderer *r,
                             struct owl_model const *model,
                             struct owl_model_mesh const *mesh) {
  int i;
  struct owl_model_mesh_data const *mesh_data;

  if (OWL_MODEL_MESH_HANDLE_NONE == mesh->handle)
    return;

  mesh_data = &model->meshes[mesh->handle];

  for (i = 0; i < mesh_data->primitives_count; ++i) {
    struct owl_model_material const *material =
        &mesh_data->primitives[i].material;
    struct owl_model_material_data const *material_data =
        &model->materials[material->handle];
  }
}

#endif

enum owl_code owl_model_submit(struct owl_renderer *r,
                               struct owl_draw_command_ubo const *ubo,
                               struct owl_model const *model) {
  int i;

  owl_model_bind_vertex_and_index_buffers_(r, model);

  {
    VkDeviceSize size;
    owl_byte *data;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(*ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = model->materials[1].set;

    vkCmdBindDescriptorSets(r->active_command_buffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  for (i = 0; i < model->roots_count; ++i)
    owl_submit_node_(r, model, &model->roots[i]);

  return OWL_SUCCESS;
}

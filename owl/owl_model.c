#include "owl_model.h"

#include "cgltf.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_types.h"
#include "owl_vector_math.h"

#include <float.h>
#include <stdio.h>

#if !defined(NDEBUG)

#define OWL_VK_CHECK(e)                                                        \
  do {                                                                         \
    VkResult const result_ = e;                                                \
    if (VK_SUCCESS != result_)                                                 \
      OWL_DEBUG_LOG("OWL_VK_CHECK(%s) result = %i\n", #e, result_);            \
    OWL_ASSERT(VK_SUCCESS == result_);                                         \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

struct owl_model_uri {
  char path[OWL_MODEL_MAX_NAME_LENGTH];
};

OWL_INTERNAL void const *
owl_resolve_gltf_accessor(struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  owl_byte const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

OWL_INTERNAL enum owl_code owl_model_uri_init(char const *src,
                                              struct owl_model_uri *uri) {
  enum owl_code code = OWL_SUCCESS;

  snprintf(uri->path, OWL_MODEL_MAX_NAME_LENGTH, "../../assets/%s", src);

  return code;
}

OWL_INTERNAL enum owl_code owl_model_images_load(struct owl_renderer *r,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)gltf->images_count; ++i) {
    struct owl_model_uri uri;
    struct owl_renderer_image_init_desc image_desc;
    struct owl_model_image_data *image_data = &model->images[i];

    if (OWL_SUCCESS != (code = owl_model_uri_init(gltf->images[i].uri, &uri))) {
      goto out;
    }

    image_desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_FILE;
    image_desc.src_path = uri.path;
    /* FIXME(samuel): if im not mistaken, gltf defines some sampler requirements
     * . Completely ignoring it for now */
    image_desc.sampler_use_default = 1;

    code = owl_renderer_image_init(r, &image_desc, &image_data->image);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  model->images_count = (owl_i32)gltf->images_count;

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_textures_load(struct owl_renderer *r, struct cgltf_data const *gltf,
                        struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  model->textures_count = (owl_i32)gltf->textures_count;

  for (i = 0; i < (owl_i32)gltf->textures_count; ++i) {
    struct cgltf_texture const *gltf_texture = &gltf->textures[i];
    model->textures[i].image.slot =
        (owl_i32)(gltf_texture->image - gltf->images);
  }

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_materials_load(struct owl_renderer *r, struct cgltf_data const *gltf,
                         struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_MATERIALS_COUNT <= (owl_i32)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->materials_count = (owl_i32)gltf->materials_count;

  for (i = 0; i < (owl_i32)gltf->materials_count; ++i) {
    struct cgltf_material const *gm = &gltf->materials[i];
    struct owl_model_material_data *md = &model->materials[i];

    OWL_ASSERT(gm->has_pbr_metallic_roughness);

    OWL_V4_COPY(gm->pbr_metallic_roughness.base_color_factor,
                md->base_color_factor);

    md->base_color_texture.slot =
        (owl_i32)(gm->pbr_metallic_roughness.base_color_texture.texture -
                  gltf->textures);

#if 0
    OWL_ASSERT(src_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (owl_i32)(src_material->normal_texture.texture - gltf->textures);
#else
    md->normal_texture.slot = md->base_color_texture.slot;
    md->physical_desc_texture.slot = md->base_color_texture.slot;
    md->occlusion_texture.slot = OWL_MODEL_NO_TEXTURE_SLOT;
    md->emissive_texture.slot = OWL_MODEL_NO_TEXTURE_SLOT;
#endif

    /* FIXME(samuel): make sure this is right */
    md->alpha_mode = (enum owl_alpha_mode)gm->alpha_mode;
    md->alpha_cutoff = gm->alpha_cutoff;
    md->double_sided = gm->double_sided;
  }

out:
  return code;
}

struct owl_model_load_state {
  owl_i32 vertices_capacity;
  owl_i32 vertices_count;
  struct owl_model_vertex *vertices;

  owl_i32 indices_capacity;
  owl_i32 indices_count;
  owl_u32 *indices;
};

OWL_INTERNAL struct cgltf_attribute const *
owl_find_gltf_attribute(struct cgltf_primitive const *prim, char const *name) {
  owl_i32 i;
  struct cgltf_attribute const *attribute = NULL;

  for (i = 0; i < (owl_i32)prim->attributes_count; ++i) {
    struct cgltf_attribute const *current = &prim->attributes[i];

    if (!OWL_STRNCMP(current->name, name, OWL_MODEL_MAX_NAME_LENGTH)) {
      attribute = current;
      goto out;
    }
  }

out:
  return attribute;
}

OWL_INTERNAL void
owl_model_load_state_find_capacities(struct cgltf_data const *gltf,
                                     struct owl_model_load_state *state) {
  owl_i32 i;
  for (i = 0; i < (owl_i32)gltf->nodes_count; ++i) {
    owl_i32 j;
    struct cgltf_node const *gn = &gltf->nodes[i];

    if (!gn->mesh) {
      continue;
    }

    for (j = 0; j < (owl_i32)gn->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attr;
      struct cgltf_primitive const *prim = &gn->mesh->primitives[j];

      attr = owl_find_gltf_attribute(prim, "POSITION");
      state->vertices_capacity += attr->data->count;
      state->indices_capacity += prim->indices->count;
    }
  }
}

OWL_INTERNAL enum owl_code
owl_model_load_state_init(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model_load_state *state) {
  owl_u64 sz;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  state->vertices_capacity = 0;
  state->indices_capacity = 0;
  state->vertices_count = 0;
  state->indices_count = 0;

  owl_model_load_state_find_capacities(gltf, state);

  sz = (owl_u64)state->vertices_capacity * sizeof(*state->vertices);
  if (!(state->vertices = OWL_MALLOC(sz))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out;
  }

  sz = (owl_u64)state->indices_capacity * sizeof(owl_u32);
  if (!(state->indices = OWL_MALLOC(sz))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out_err_vertices_free;
  }

  goto out;

out_err_vertices_free:
  OWL_FREE(state->vertices);

out:
  return code;
}

OWL_INTERNAL void
owl_model_load_state_deinit(struct owl_renderer *r,
                            struct owl_model_load_state *state) {
  OWL_UNUSED(r);

  OWL_FREE(state->indices);
  OWL_FREE(state->vertices);
}

OWL_INTERNAL enum owl_code
owl_model_node_load(struct owl_renderer *r, struct cgltf_data const *gltf,
                    struct cgltf_node const *gltf_node,
                    struct owl_model_load_state *state,
                    struct owl_model *model) {
  owl_i32 i;
  struct owl_model_node node;
  struct owl_model_node_data *node_data;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  node.slot = (owl_i32)(gltf_node - gltf->nodes);

  if (OWL_MODEL_MAX_NODES_COUNT <= node.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  node_data = &model->nodes[node.slot];

  if (gltf_node->parent) {
    node_data->parent.slot = (owl_i32)(gltf_node->parent - gltf->nodes);
  } else {
    node_data->parent.slot = OWL_MODEL_NODE_NO_PARENT_SLOT;
  }

  if (OWL_MODEL_NODE_MAX_CHILDREN_COUNT <= (owl_i32)gltf_node->children_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  node_data->children_count = (owl_i32)gltf_node->children_count;

  for (i = 0; i < (owl_i32)gltf_node->children_count; ++i) {
    node_data->children[i].slot =
        (owl_i32)(gltf_node->children[i] - gltf->nodes);
  }

  if (gltf_node->name) {
    OWL_STRNCPY(node_data->name, gltf_node->name, OWL_MODEL_MAX_NAME_LENGTH);
  } else {
    OWL_STRNCPY(node_data->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
  }

  if (gltf_node->has_translation) {
    OWL_V3_COPY(gltf_node->translation, node_data->translation);
  } else {
    OWL_V3_ZERO(node_data->translation);
  }

  if (gltf_node->has_rotation) {
    OWL_V4_COPY(gltf_node->rotation, node_data->rotation);
  } else {
    OWL_V4_ZERO(node_data->rotation);
  }

  if (gltf_node->has_scale) {
    OWL_V3_COPY(gltf_node->scale, node_data->scale);
  } else {
    OWL_V3_SET(1.0F, 1.0F, 1.0F, node_data->scale);
  }

  if (gltf_node->has_matrix) {
    OWL_M4_COPY_V16(gltf_node->matrix, node_data->matrix);
  } else {
    OWL_M4_IDENTITY(node_data->matrix);
  }

  if (gltf_node->skin) {
    node_data->skin.slot = (owl_i32)(gltf_node->skin - gltf->skins);
  } else {
    node_data->skin.slot = OWL_MODEL_NODE_NO_SKIN_SLOT;
  }

  if (gltf_node->mesh) {
    struct cgltf_mesh const *gltf_mesh;
    struct owl_model_mesh_data *mesh_data;

    node_data->mesh.slot = model->meshes_count++;

    if (OWL_MODEL_MAX_MESHES_COUNT <= node_data->mesh.slot) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    gltf_mesh = gltf_node->mesh;
    mesh_data = &model->meshes[node_data->mesh.slot];

    mesh_data->primitives_count = (owl_i32)gltf_mesh->primitives_count;

    for (i = 0; i < (owl_i32)gltf_mesh->primitives_count; ++i) {
      owl_i32 j;
      owl_i32 vertices_count = 0;
      owl_i32 indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv0 = NULL;
      float const *uv1 = NULL;
      owl_u16 const *joints0 = NULL;
      float const *weights0 = NULL;
      struct cgltf_attribute const *attr = NULL;
      struct owl_model_primitive *primitive = NULL;
      struct owl_model_primitive_data *primitive_data = NULL;
      struct cgltf_primitive const *gltf_primitive = &gltf_mesh->primitives[i];

      primitive = &mesh_data->primitives[i];
      primitive->slot = model->primitives_count++;

      if (OWL_MODEL_MAX_PRIMITIVES_COUNT <= primitive->slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      primitive_data = &model->primitives[primitive->slot];

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "POSITION"))) {
        position = owl_resolve_gltf_accessor(attr->data);
        vertices_count = (owl_i32)attr->data->count;
      }

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "NORMAL"))) {
        normal = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "TEXCOORD_0"))) {
        uv0 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "TEXCOORD_1"))) {
        uv1 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "JOINTS_0"))) {
        joints0 = owl_resolve_gltf_accessor(attr->data);
      }

      if ((attr = owl_find_gltf_attribute(gltf_primitive, "WEIGHTS_0"))) {
        weights0 = owl_resolve_gltf_accessor(attr->data);
      }

      for (j = 0; j < vertices_count; ++j) {
        owl_i32 offset = state->vertices_count;
        struct owl_model_vertex *vertex = &state->vertices[offset + j];

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

      indices_count = (owl_i32)gltf_primitive->indices->count;

      switch (gltf_primitive->indices->component_type) {
      case cgltf_component_type_r_32u: {
        owl_u32 const *indices;
        owl_i32 const offset = state->indices_count;

        indices = owl_resolve_gltf_accessor(gltf_primitive->indices);

        for (j = 0; j < (owl_i32)gltf_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u32)state->vertices_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        owl_u16 const *indices;
        owl_i32 const offset = state->indices_count;

        indices = owl_resolve_gltf_accessor(gltf_primitive->indices);

        for (j = 0; j < (owl_i32)gltf_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u16)state->vertices_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        owl_u8 const *indices;
        owl_i32 const offset = state->indices_count;

        indices = owl_resolve_gltf_accessor(gltf_primitive->indices);

        for (j = 0; j < (owl_i32)gltf_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u8)state->vertices_count;
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_UNKNOWN;
        goto out;
      }

      primitive_data->first = (owl_u32)state->indices_count;
      primitive_data->count = (owl_u32)indices_count;
      primitive_data->material.slot =
          (owl_i32)(gltf_primitive->material - gltf->materials);

      state->indices_count += indices_count;
      state->vertices_count += vertices_count;
    }
  }

out:
  return code;
}

OWL_INTERNAL void
owl_model_buffers_load(struct owl_renderer *r,
                       struct owl_model_load_state const *desc,
                       struct owl_model *model) {
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_frame_heap_is_offset_clear(r));
  OWL_ASSERT(desc->vertices_count == desc->vertices_capacity);
  OWL_ASSERT(desc->indices_count == desc->indices_capacity);

  {
    owl_u64 sz;

    sz = (owl_u64)desc->vertices_capacity * sizeof(struct owl_model_vertex);
    owl_renderer_frame_heap_submit(r, sz, desc->vertices, &vref);

    sz = (owl_u64)desc->indices_capacity * sizeof(owl_u32);
    owl_renderer_frame_heap_submit(r, sz, desc->indices, &iref);
  }

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = (owl_u64)desc->vertices_count * sizeof(struct owl_model_vertex);
    info.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &info, NULL, &model->vertices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements(r->device, model->vertices_buffer,
                                  &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &info, NULL, &model->vertices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->vertices_buffer,
                                    model->vertices_memory, 0));
  }

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = (owl_u64)desc->indices_count * sizeof(owl_u32);
    info.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &info, NULL, &model->indices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements(r->device, model->indices_buffer,
                                  &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &info, NULL, &model->indices_memory));

    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->indices_buffer,
                                    model->indices_memory, 0));
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_init(r))) {
    goto out;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_begin(r))) {
    goto out;
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = vref.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)desc->vertices_count * sizeof(struct owl_model_vertex);

    vkCmdCopyBuffer(r->immidiate_command_buffer, vref.buffer,
                    model->vertices_buffer, 1, &copy);
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = iref.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)desc->indices_count * sizeof(owl_u32);

    vkCmdCopyBuffer(r->immidiate_command_buffer, iref.buffer,
                    model->indices_buffer, 1, &copy);
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_end(r))) {
    goto out;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_submit(r))) {
    goto out;
  }

  owl_renderer_immidiate_command_buffer_deinit(r);

  owl_renderer_frame_heap_clear_offset(r);

out:
  OWL_ASSERT(OWL_SUCCESS == code);
}

OWL_INTERNAL enum owl_code owl_model_nodes_load(struct owl_renderer *r,
                                                struct cgltf_data const *gltf,
                                                struct owl_model *model) {
  owl_i32 i;
  struct owl_model_load_state state;
  struct cgltf_scene const *gs;
  enum owl_code code = OWL_SUCCESS;

  gs = gltf->scene;

  code = owl_model_load_state_init(r, gltf, &state);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  for (i = 0; i < OWL_MODEL_MAX_NODES_COUNT; ++i) {
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].parent.slot = -1;
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].skin.slot = -1;
  }

  if (OWL_MODEL_MAX_NODES_COUNT <= (owl_i32)gltf->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out_err_deinit_load_state;
  }

  model->nodes_count = (owl_i32)gltf->nodes_count;

  for (i = 0; i < (owl_i32)gltf->nodes_count; ++i) {
    code = owl_model_node_load(r, gltf, &gltf->nodes[i], &state, model);

    if (OWL_SUCCESS != code) {
      goto out_err_deinit_load_state;
    }
  }

  if (OWL_MODEL_MAX_NODE_ROOTS_COUNT <= (owl_i32)gs->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out_err_deinit_load_state;
  }

  model->roots_count = (owl_i32)gs->nodes_count;

  for (i = 0; i < (owl_i32)gs->nodes_count; ++i) {
    model->roots[i].slot = (owl_i32)(gs->nodes[i] - gltf->nodes);
  }

  owl_model_buffers_load(r, &state, model);

out_err_deinit_load_state:
  owl_model_load_state_deinit(r, &state);

out:
  return code;
}

OWL_INTERNAL enum owl_code owl_model_skins_load(struct owl_renderer *r,
                                                struct cgltf_data const *gltf,
                                                struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_SKINS_COUNT <= (owl_i32)gltf->skins_count) {
    OWL_ASSERT(0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->skins_count = (owl_i32)gltf->skins_count;

  for (i = 0; i < (owl_i32)gltf->skins_count; ++i) {
    owl_i32 j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *gltf_skin = &gltf->skins[i];
    struct owl_model_skin_data *skin_data = &model->skins[i];

    if (gltf_skin->name) {
      OWL_STRNCPY(skin_data->name, gltf_skin->name, OWL_MODEL_MAX_NAME_LENGTH);
    } else {
      OWL_STRNCPY(skin_data->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
    }

    skin_data->skeleton_root.slot =
        (owl_i32)(gltf_skin->skeleton - gltf->nodes);

    if (OWL_MODEL_SKIN_MAX_JOINTS_COUNT <= (owl_i32)gltf_skin->joints_count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    skin_data->joints_count = (owl_i32)gltf_skin->joints_count;

    for (j = 0; j < (owl_i32)gltf_skin->joints_count; ++j) {
      skin_data->joints[j].slot = (owl_i32)(gltf_skin->joints[j] - gltf->nodes);

      OWL_ASSERT(!OWL_STRNCMP(model->nodes[skin_data->joints[j].slot].name,
                              gltf_skin->joints[j]->name,
                              OWL_MODEL_MAX_NAME_LENGTH));
    }

    inverse_bind_matrices =
        owl_resolve_gltf_accessor(gltf_skin->inverse_bind_matrices);

    for (j = 0; j < (owl_i32)gltf_skin->inverse_bind_matrices->count; ++j) {
      OWL_M4_COPY(inverse_bind_matrices[j],
                  skin_data->inverse_bind_matrices[j]);
    }

    {
      VkBufferCreateInfo info;

      skin_data->ssbo_buffer_size = sizeof(struct owl_model_skin_ssbo_data);

      info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.size = skin_data->ssbo_buffer_size;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        OWL_VK_CHECK(vkCreateBuffer(r->device, &info, NULL,
                                    &skin_data->ssbo_buffers[j]));
      }
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo info;

      vkGetBufferMemoryRequirements(r->device, skin_data->ssbo_buffers[0],
                                    &requirements);

      skin_data->ssbo_buffer_alignment = requirements.alignment;
      skin_data->ssbo_buffer_aligned_size =
          OWL_ALIGNU2(skin_data->ssbo_buffer_size, requirements.alignment);

      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = NULL;
      info.allocationSize = skin_data->ssbo_buffer_aligned_size *
                            OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      info.memoryTypeIndex = owl_renderer_find_memory_type(
          r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

      OWL_VK_CHECK(
          vkAllocateMemory(r->device, &info, NULL, &skin_data->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        OWL_VK_CHECK(vkBindBufferMemory(
            r->device, skin_data->ssbo_buffers[j], skin_data->ssbo_memory,
            (owl_u64)j * skin_data->ssbo_buffer_aligned_size));
      }
    }

    {
      VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
      VkDescriptorSetAllocateInfo info;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        layouts[j] = r->vertex_ssbo_set_layout;
      }

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = r->common_set_pool;
      info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      info.pSetLayouts = layouts;

      OWL_VK_CHECK(
          vkAllocateDescriptorSets(r->device, &info, skin_data->ssbo_sets));
    }

    {
      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        VkDescriptorBufferInfo buffer_info;
        VkWriteDescriptorSet write;

        buffer_info.buffer = skin_data->ssbo_buffers[j];
        buffer_info.offset = 0;
        buffer_info.range = skin_data->ssbo_buffer_size;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = NULL;
        write.dstSet = skin_data->ssbo_sets[j];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pImageInfo = NULL;
        write.pBufferInfo = &buffer_info;
        write.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
      }
    }

    {
      owl_i32 k;
      void *data;

      vkMapMemory(r->device, skin_data->ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data);

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        owl_u64 offset = (owl_u64)j * skin_data->ssbo_buffer_aligned_size;
        owl_byte *ssbo = &((owl_byte *)data)[offset];
        skin_data->ssbo_datas[j] = (struct owl_model_skin_ssbo_data *)ssbo;
      }

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        struct owl_model_skin_ssbo_data *ssbo = skin_data->ssbo_datas[j];

        OWL_M4_IDENTITY(ssbo->matrix);

        for (k = 0; k < (owl_i32)gltf_skin->inverse_bind_matrices->count; ++k) {
          OWL_M4_COPY(inverse_bind_matrices[k], ssbo->joint_matices[k]);
        }

        ssbo->joint_matrices_count = skin_data->joints_count;
      }
    }
  }
out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_animations_load(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_ANIMATIONS_COUNT <= (owl_i32)gltf->animations_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->animations_count = (owl_i32)gltf->animations_count;

  for (i = 0; i < (owl_i32)gltf->animations_count; ++i) {
    owl_i32 j;
    struct cgltf_animation const *gltf_animation;
    struct owl_model_animation_data *animation_data;

    gltf_animation = &gltf->animations[i];
    animation_data = &model->animations[i];

    animation_data->current_time = 0.0F;

    if (OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT <=
        (owl_i32)gltf_animation->samplers_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    animation_data->samplers_count = (owl_i32)gltf_animation->samplers_count;

    animation_data->begin = FLT_MAX;
    animation_data->end = FLT_MIN;

    for (j = 0; j < (owl_i32)gltf_animation->samplers_count; ++j) {
      owl_i32 k;
      float const *inputs;
      struct cgltf_animation_sampler const *gltf_sampler;
      struct owl_model_animation_sampler sampler;
      struct owl_model_animation_sampler_data *sampler_data;

      gltf_sampler = &gltf_animation->samplers[j];

      sampler.slot = model->animation_samplers_count++;

      if (OWL_MODEL_MAX_SAMPLERS_COUNT <= sampler.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler_data = &model->animation_samplers[sampler.slot];

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_INPUTS_COUNT <=
          (owl_i32)gltf_sampler->input->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler_data->inputs_count = (owl_i32)gltf_sampler->input->count;

      inputs = owl_resolve_gltf_accessor(gltf_sampler->input);

      for (k = 0; k < (owl_i32)gltf_sampler->input->count; ++k) {
        float const input = inputs[k];

        sampler_data->inputs[k] = input;

        if (input < animation_data->begin) {
          animation_data->begin = input;
        }

        if (input > animation_data->end) {
          animation_data->end = input;
        }
      }

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT <=
          (owl_i32)gltf_sampler->output->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler_data->outputs_count = (owl_i32)gltf_sampler->output->count;

      switch (gltf_sampler->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor(gltf_sampler->output);

        for (k = 0; k < (owl_i32)gltf_sampler->output->count; ++k) {
          OWL_V4_ZERO(sampler_data->outputs[k]);
          OWL_V3_COPY(outputs[k], sampler_data->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor(gltf_sampler->output);
        for (k = 0; k < (owl_i32)gltf_sampler->output->count; ++k) {
          OWL_V4_COPY(outputs[k], sampler_data->outputs[k]);
        }
      } break;

      case cgltf_type_invalid:
      case cgltf_type_scalar:
      case cgltf_type_vec2:
      case cgltf_type_mat2:
      case cgltf_type_mat3:
      case cgltf_type_mat4:
      default:
        code = OWL_ERROR_UNKNOWN;
        goto out;
      }

      sampler_data->interpolation = (owl_i32)gltf_sampler->interpolation;

      animation_data->samplers[j].slot = sampler.slot;
    }

    if (OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT <=
        (owl_i32)gltf_animation->channels_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    animation_data->channels_count = (owl_i32)gltf_animation->channels_count;

    for (j = 0; j < (owl_i32)gltf_animation->channels_count; ++j) {
      struct cgltf_animation_channel const *gltf_channel;
      struct owl_model_animation_channel channel;
      struct owl_model_animation_channel_data *channel_data;

      gltf_channel = &gltf_animation->channels[j];

      channel.slot = model->animation_channels_count++;

      if (OWL_MODEL_MAX_CHANNELS_COUNT <= channel.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      channel_data = &model->animation_channels[channel.slot];

      channel_data->path = (owl_i32)gltf_channel->target_path;
      channel_data->node.slot =
          (owl_i32)(gltf_channel->target_node - gltf->nodes);
      channel_data->animation_sampler.slot =
          animation_data
              ->samplers[(gltf_channel->sampler - gltf_animation->samplers)]
              .slot;

      animation_data->channels[j].slot = channel.slot;
    }
  }

out:
  return code;
}

enum owl_code owl_model_init(struct owl_renderer *r, char const *path,
                             struct owl_model *model) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_frame_heap_is_offset_clear(r));

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));

  model->roots_count = 0;
  model->nodes_count = 0;
  model->images_count = 0;
  model->textures_count = 0;
  model->materials_count = 0;
  model->meshes_count = 0;
  model->primitives_count = 0;
  model->skins_count = 0;
  model->animation_samplers_count = 0;
  model->animation_channels_count = 0;
  model->animations_count = 0;
#if 0
  model->active_animation.slot = OWL_MODEL_NO_ANIMATION_SLOT;
#else
  model->active_animation.slot = 0;
#endif

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data)) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    code = OWL_ERROR_UNKNOWN;
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_images_load(r, data, model))) {
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_textures_load(r, data, model))) {
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_materials_load(r, data, model))) {
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_nodes_load(r, data, model))) {
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_skins_load(r, data, model))) {
    goto out_err_data_free;
  }

  if (OWL_SUCCESS != (code = owl_model_animations_load(r, data, model))) {
    goto out_err_data_free;
  }

out_err_data_free:
  cgltf_free(data);

  owl_renderer_frame_heap_clear_offset(r);

out:
  return code;
}

void owl_model_deinit(struct owl_renderer *r, struct owl_model *model) {
  owl_i32 i;

  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeMemory(r->device, model->indices_memory, NULL);
  vkDestroyBuffer(r->device, model->indices_buffer, NULL);
  vkFreeMemory(r->device, model->vertices_memory, NULL);
  vkDestroyBuffer(r->device, model->vertices_buffer, NULL);

  for (i = 0; i < model->images_count; ++i) {
    owl_renderer_image_deinit(r, &model->images[i].image);
  }

  for (i = 0; i < model->skins_count; ++i) {
    owl_i32 j;

    vkFreeDescriptorSets(r->device, r->common_set_pool,
                         OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                         model->skins[i].ssbo_sets);

    vkFreeMemory(r->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
      vkDestroyBuffer(r->device, model->skins[i].ssbo_buffers[j], NULL);
    }
  }
}

OWL_INTERNAL void
owl_model_resolve_local_node_matrix(struct owl_model const *model,
                                    struct owl_model_node const *node,
                                    owl_m4 matrix) {
  owl_m4 tmp;
  struct owl_model_node_data const *dst_node;

  dst_node = &model->nodes[node->slot];

  OWL_M4_IDENTITY(matrix);
  owl_m4_translate(dst_node->translation, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_q4_as_m4(dst_node->rotation, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_m4_scale(tmp, dst_node->scale, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  owl_m4_multiply(matrix, dst_node->matrix, matrix);
}

OWL_INTERNAL void
owl_model_resolve_node_matrix(struct owl_model const *model,
                              struct owl_model_node const *node,
                              owl_m4 matrix) {
  struct owl_model_node parent;

  owl_model_resolve_local_node_matrix(model, node, matrix);

  for (parent.slot = model->nodes[node->slot].parent.slot;
       OWL_MODEL_NODE_NO_PARENT_SLOT != parent.slot;
       parent.slot = model->nodes[parent.slot].parent.slot) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix(model, &parent, local);
    owl_m4_multiply(local, matrix, matrix);
  }
}

OWL_INTERNAL void
owl_model_node_joints_update(struct owl_model *model, owl_i32 frame,
                             struct owl_model_node const *node) {
  owl_i32 i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin_data const *dst_skin;
  struct owl_model_node_data const *dst_node;

  dst_node = &model->nodes[node->slot];

  for (i = 0; i < dst_node->children_count; ++i) {
    owl_model_node_joints_update(model, frame, &dst_node->children[i]);
  }

  if (OWL_MODEL_NODE_NO_SKIN_SLOT == dst_node->skin.slot) {
    goto out;
  }

  dst_skin = &model->skins[dst_node->skin.slot];

  owl_model_resolve_node_matrix(model, node, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < dst_skin->joints_count; ++i) {
    struct owl_model_skin_ssbo_data *ssbo = dst_skin->ssbo_datas[frame];

    owl_model_resolve_node_matrix(model, &dst_skin->joints[i], tmp);
    owl_m4_multiply(tmp, dst_skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp, ssbo->joint_matices[i]);
  }

out:
  return;
}

#define OWL_MODEL_ANIMATION_INTERPOLATION_TYPE_LINEAR                          \
  cgltf_interpolation_type_linear

#define OWL_MODEL_ANIMATION_PATH_TYPE_TRANSLATION                              \
  cgltf_animation_path_type_translation

#define OWL_MODEL_ANIMATION_PATH_TYPE_ROTATION                                 \
  cgltf_animation_path_type_rotation

#define OWL_MODEL_ANIMATION_PATH_TYPE_SCALE cgltf_animation_path_type_scale

enum owl_code owl_model_update_animation(struct owl_model *model,
                                         owl_i32 animation, owl_i32 frame,
                                         float dt) {
  owl_i32 i;
  struct owl_model_animation_data *animation_data;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_NO_ANIMATION_SLOT == animation) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  animation_data = &model->animations[animation];

  if (animation_data->end < (animation_data->current_time += dt)) {
    animation_data->current_time -= animation_data->end;
  }

  for (i = 0; i < animation_data->channels_count; ++i) {
    owl_i32 j;
    struct owl_model_animation_channel channel;
    struct owl_model_animation_sampler sampler;
    struct owl_model_node_data *node_data;
    struct owl_model_animation_channel_data const *channel_data;
    struct owl_model_animation_sampler_data const *sampler_data;

    channel.slot = animation_data->channels[i].slot;
    channel_data = &model->animation_channels[channel.slot];
    sampler.slot = channel_data->animation_sampler.slot;
    sampler_data = &model->animation_samplers[sampler.slot];
    node_data = &model->nodes[channel_data->node.slot];

    if (OWL_MODEL_ANIMATION_INTERPOLATION_TYPE_LINEAR !=
        sampler_data->interpolation) {
      OWL_DEBUG_LOG("skipping channel %i\n", i);
      continue;
    }

    for (j = 0; j < sampler_data->inputs_count - 1; ++j) {
      float const i0 = sampler_data->inputs[j];
      float const i1 = sampler_data->inputs[j + 1];
      float const ct = animation_data->current_time;
      float const a = (ct - i0) / (i1 - i0);

      if (!((ct >= i0) && (ct <= i1))) {
        continue;
      }

      switch (channel_data->path) {
      case OWL_MODEL_ANIMATION_PATH_TYPE_TRANSLATION: {
        owl_v3_mix(sampler_data->outputs[j], sampler_data->outputs[j + 1], a,
                   node_data->translation);
      } break;

      case OWL_MODEL_ANIMATION_PATH_TYPE_ROTATION: {
        owl_v4_quat_slerp(sampler_data->outputs[j],
                          sampler_data->outputs[j + 1], a, node_data->rotation);
        owl_v4_normalize(node_data->rotation, node_data->rotation);
      } break;

      case OWL_MODEL_ANIMATION_PATH_TYPE_SCALE: {
        owl_v3_mix(sampler_data->outputs[j], sampler_data->outputs[j + 1], a,
                   node_data->scale);
      } break;

      default:
        OWL_ASSERT(0 && "unexpected path");
        code = OWL_ERROR_UNKNOWN;
        goto out;
      }
    }
  }

  for (i = 0; i < model->roots_count; ++i) {
    owl_model_node_joints_update(model, frame, &model->roots[i]);
  }

out:
  return code;
}

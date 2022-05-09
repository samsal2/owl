#include "owl_model.h"

#include "cgltf.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#include <float.h>
#include <stdio.h>

#if !defined(NDEBUG)

#define OWL_VK_CHECK(e)                                                       \
  do {                                                                        \
    VkResult const result_ = e;                                               \
    if (VK_SUCCESS != result_)                                                \
      OWL_DEBUG_LOG ("OWL_VK_CHECK(%s) result = %i\n", #e, result_);          \
    owl_assert (VK_SUCCESS == result_);                                       \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

struct owl_model_uri {
  char path[OWL_MODEL_MAX_NAME_LENGTH];
};

owl_private void const *
owl_resolve_gltf_accessor (struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  owl_byte const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

owl_private enum owl_code
owl_model_uri_init (char const *src, struct owl_model_uri *uri) {
  enum owl_code code = OWL_SUCCESS;

  snprintf (uri->path, OWL_MODEL_MAX_NAME_LENGTH, "../../assets/%s", src);

  return code;
}

owl_private enum owl_code
owl_model_images_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                       struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)gltf->images_count; ++i) {
    struct owl_model_uri uri;
    struct owl_renderer_image_desc desc;
    struct owl_model_image *image = &model->images[i];

    code = owl_model_uri_init (gltf->images[i].uri, &uri);
    if (OWL_SUCCESS != code) {
      goto out;
    }

    desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_FILE;
    desc.src_path = uri.path;
    /* FIXME(samuel): if im not mistaken, gltf defines some sampler
     * requirements . Completely ignoring it for now */
    desc.sampler_use_default = 1;

    code = owl_renderer_image_init (r, &desc, &image->renderer_image);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  model->image_count = (owl_i32)gltf->images_count;

out:
  return code;
}

owl_private enum owl_code
owl_model_textures_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                         struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  model->texture_count = (owl_i32)gltf->textures_count;

  for (i = 0; i < (owl_i32)gltf->textures_count; ++i) {
    struct cgltf_texture const *gt = &gltf->textures[i];
    model->textures[i].model_image = (owl_i32)(gt->image - gltf->images);
  }

  return code;
}

owl_private enum owl_code
owl_model_materials_load (struct owl_renderer *r,
                          struct cgltf_data const *gltf,
                          struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  if (OWL_MODEL_MAX_MATERIAL_COUNT <= (owl_i32)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->material_count = (owl_i32)gltf->materials_count;

  for (i = 0; i < (owl_i32)gltf->materials_count; ++i) {
    struct cgltf_material const *gm = &gltf->materials[i];
    struct owl_model_material *material = &model->materials[i];

    owl_assert (gm->has_pbr_metallic_roughness);

    owl_v4_copy (gm->pbr_metallic_roughness.base_color_factor,
                 material->base_color_factor);

    material->base_color_texture =
        (owl_model_texture_id)(gm->pbr_metallic_roughness.base_color_texture
                                   .texture -
                               gltf->textures);

#if 0
    owl_assert(src_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (owl_i32)(src_material->normal_texture.texture - gltf->textures);
#else
    material->normal_texture = material->base_color_texture;
    material->physical_desc_texture = material->base_color_texture;
    material->occlusion_texture = OWL_MODEL_TEXTURE_NONE;
    material->emissive_texture = OWL_MODEL_TEXTURE_NONE;
#endif

    /* FIXME(samuel): make sure this is right */
    material->alpha_mode = (enum owl_alpha_mode)gm->alpha_mode;
    material->alpha_cutoff = gm->alpha_cutoff;
    material->double_sided = gm->double_sided;
  }

out:
  return code;
}

struct owl_model_load_state {
  owl_i32 vertex_capacity;
  owl_i32 vertex_count;
  struct owl_model_vertex *vertices;

  owl_i32 index_capacity;
  owl_i32 index_count;
  owl_u32 *indices;
};

owl_private struct cgltf_attribute const *
owl_find_gltf_attribute (struct cgltf_primitive const *p, char const *name) {
  owl_i32 i;
  struct cgltf_attribute const *attr = NULL;

  for (i = 0; i < (owl_i32)p->attributes_count; ++i) {
    struct cgltf_attribute const *current = &p->attributes[i];

    if (!owl_strncmp (current->name, name, OWL_MODEL_MAX_NAME_LENGTH)) {
      attr = current;
      goto out;
    }
  }

out:
  return attr;
}

owl_private void
owl_model_load_state_find_capacities (struct cgltf_data const *gltf,
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
      struct cgltf_primitive const *p = &gn->mesh->primitives[j];

      attr = owl_find_gltf_attribute (p, "POSITION");
      state->vertex_capacity += attr->data->count;
      state->index_capacity += p->indices->count;
    }
  }
}

owl_private enum owl_code
owl_model_load_state_init (struct owl_renderer *r,
                           struct cgltf_data const *gltf,
                           struct owl_model_load_state *state) {
  owl_u64 sz;
  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  state->vertex_capacity = 0;
  state->index_capacity = 0;
  state->vertex_count = 0;
  state->index_count = 0;

  owl_model_load_state_find_capacities (gltf, state);

  sz = (owl_u64)state->vertex_capacity * sizeof (*state->vertices);
  state->vertices = owl_malloc (sz);
  if (!state->vertices) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out;
  }

  sz = (owl_u64)state->index_capacity * sizeof (owl_u32);
  state->indices = owl_malloc (sz);
  if (!state->indices) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out_err_vertices_free;
  }

  goto out;

out_err_vertices_free:
  owl_free (state->vertices);

out:
  return code;
}

owl_private void
owl_model_load_state_deinit (struct owl_renderer *r,
                             struct owl_model_load_state *state) {
  owl_unused (r);

  owl_free (state->indices);
  owl_free (state->vertices);
}

owl_private enum owl_code
owl_model_node_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                     struct cgltf_node const *gn,
                     struct owl_model_load_state *state,
                     struct owl_model *model) {
  owl_i32 i;
  owl_model_node_id nid;
  struct owl_model_node *node;

  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  nid = (owl_model_node_id)(gn - gltf->nodes);

  if (OWL_MODEL_MAX_NODE_COUNT <= nid) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  node = &model->nodes[nid];

  if (gn->parent) {
    node->parent = (owl_model_node_id)(gn->parent - gltf->nodes);
  } else {
    node->parent = OWL_MODEL_NODE_PARENT_NONE;
  }

  if (OWL_MODEL_NODE_MAX_CHILDREN_COUNT <= (owl_i32)gn->children_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  node->child_count = (owl_i32)gn->children_count;

  for (i = 0; i < (owl_i32)gn->children_count; ++i) {
    node->children[i] = (owl_model_node_id)(gn->children[i] - gltf->nodes);
  }

  if (gn->name) {
    owl_strncpy (node->name, gn->name, OWL_MODEL_MAX_NAME_LENGTH);
  } else {
    owl_strncpy (node->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
  }

  if (gn->has_translation) {
    owl_v3_copy (gn->translation, node->translation);
  } else {
    owl_v3_zero (node->translation);
  }

  if (gn->has_rotation) {
    owl_v4_copy (gn->rotation, node->rotation);
  } else {
    owl_v4_zero (node->rotation);
  }

  if (gn->has_scale) {
    owl_v3_copy (gn->scale, node->scale);
  } else {
    owl_v3_set (node->scale, 1.0F, 1.0F, 1.0F);
  }

  if (gn->has_matrix) {
    owl_m4_copy_v16 (gn->matrix, node->matrix);
  } else {
    owl_m4_identity (node->matrix);
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

    if (OWL_MODEL_MAX_MESH_COUNT <= node->mesh) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    gm = gn->mesh;
    md = &model->meshes[node->mesh];

    md->primitive_count = (owl_i32)gm->primitives_count;

    for (i = 0; i < (owl_i32)gm->primitives_count; ++i) {
      owl_i32 j;
      owl_i32 vertex_count = 0;
      owl_i32 index_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv0 = NULL;
      float const *uv1 = NULL;
      owl_u16 const *joints0 = NULL;
      float const *weights0 = NULL;
      struct cgltf_attribute const *attr = NULL;
      struct owl_model_primitive *pd = NULL;
      struct cgltf_primitive const *gp = &gm->primitives[i];

      md->primitives[i] = model->primitive_count++;

      if (OWL_MODEL_MAX_PRIMITIVE_COUNT <= md->primitives[i]) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      pd = &model->primitives[md->primitives[i]];

      if ((attr = owl_find_gltf_attribute (gp, "POSITION"))) {
        position = owl_resolve_gltf_accessor (attr->data);
        vertex_count = (owl_i32)attr->data->count;
      }

      if ((attr = owl_find_gltf_attribute (gp, "NORMAL"))) {
        normal = owl_resolve_gltf_accessor (attr->data);
      }

      if ((attr = owl_find_gltf_attribute (gp, "TEXCOORD_0"))) {
        uv0 = owl_resolve_gltf_accessor (attr->data);
      }

      if ((attr = owl_find_gltf_attribute (gp, "TEXCOORD_1"))) {
        uv1 = owl_resolve_gltf_accessor (attr->data);
      }

      if ((attr = owl_find_gltf_attribute (gp, "JOINTS_0"))) {
        joints0 = owl_resolve_gltf_accessor (attr->data);
      }

      if ((attr = owl_find_gltf_attribute (gp, "WEIGHTS_0"))) {
        weights0 = owl_resolve_gltf_accessor (attr->data);
      }

      for (j = 0; j < vertex_count; ++j) {
        owl_i32 offset = state->vertex_count;
        struct owl_model_vertex *vertex = &state->vertices[offset + j];

        owl_v3_copy (&position[j * 3], vertex->position);

        if (normal) {
          owl_v3_copy (&normal[j * 3], vertex->normal);
        } else {
          owl_v3_zero (vertex->normal);
        }

        if (uv0) {
          owl_v2_copy (&uv0[j * 2], vertex->uv0);
        } else {
          owl_v2_zero (vertex->uv0);
        }

        if (uv1) {
          owl_v2_copy (&uv1[j * 2], vertex->uv1);
        } else {
          owl_v2_zero (vertex->uv1);
        }

        if (joints0 && weights0) {
          owl_v4_copy (&joints0[j * 4], vertex->joints0);
        } else {
          owl_v4_zero (vertex->joints0);
        }

        if (joints0 && weights0) {
          owl_v4_copy (&weights0[j * 4], vertex->weights0);
        } else {
          owl_v4_zero (vertex->weights0);
        }
      }

      index_count = (owl_i32)gp->indices->count;

      switch (gp->indices->component_type) {
      case cgltf_component_type_r_32u: {
        owl_u32 const *indices;
        owl_i32 const offset = state->index_count;

        indices = owl_resolve_gltf_accessor (gp->indices);

        for (j = 0; j < (owl_i32)gp->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u32)state->vertex_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        owl_u16 const *indices;
        owl_i32 const offset = state->index_count;

        indices = owl_resolve_gltf_accessor (gp->indices);

        for (j = 0; j < (owl_i32)gp->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u16)state->vertex_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        owl_u8 const *indices;
        owl_i32 const offset = state->index_count;

        indices = owl_resolve_gltf_accessor (gp->indices);

        for (j = 0; j < (owl_i32)gp->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u8)state->vertex_count;
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_UNKNOWN;
        goto out;
      }

      pd->first = (owl_u32)state->index_count;
      pd->count = (owl_u32)index_count;
      pd->material = (owl_model_material_id)(gp->material - gltf->materials);
      state->index_count += index_count;
      state->vertex_count += vertex_count;
    }
  }

out:
  return code;
}

owl_private void
owl_model_buffers_load (struct owl_renderer *r,
                        struct owl_model_load_state const *state,
                        struct owl_model *model) {
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (owl_renderer_frame_heap_offset_is_clear (r));
  owl_assert (state->vertex_count == state->vertex_capacity);
  owl_assert (state->index_count == state->index_capacity);

  {
    owl_u64 sz;

    sz = (owl_u64)state->vertex_capacity * sizeof (struct owl_model_vertex);
    owl_renderer_frame_heap_submit (r, sz, state->vertices, &vref);

    sz = (owl_u64)state->index_capacity * sizeof (owl_u32);
    owl_renderer_frame_heap_submit (r, sz, state->indices, &iref);
  }

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size =
        (owl_u64)state->vertex_count * sizeof (struct owl_model_vertex);
    info.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK (
        vkCreateBuffer (r->device, &info, NULL, &model->vertices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements (r->device, model->vertices_buffer,
                                   &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type (
        r, requirements.memoryTypeBits, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK (
        vkAllocateMemory (r->device, &info, NULL, &model->vertices_memory));
    OWL_VK_CHECK (vkBindBufferMemory (r->device, model->vertices_buffer,
                                      model->vertices_memory, 0));
  }

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = (owl_u64)state->index_count * sizeof (owl_u32);
    info.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK (
        vkCreateBuffer (r->device, &info, NULL, &model->indices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements (r->device, model->indices_buffer,
                                   &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type (
        r, requirements.memoryTypeBits, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK (
        vkAllocateMemory (r->device, &info, NULL, &model->indices_memory));

    OWL_VK_CHECK (vkBindBufferMemory (r->device, model->indices_buffer,
                                      model->indices_memory, 0));
  }

  code = owl_renderer_immidiate_command_buffer_init (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_immidiate_command_buffer_begin (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = vref.offset;
    copy.dstOffset = 0;
    copy.size =
        (owl_u64)state->vertex_count * sizeof (struct owl_model_vertex);

    vkCmdCopyBuffer (r->immidiate_command_buffer, vref.buffer,
                     model->vertices_buffer, 1, &copy);
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = iref.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)state->index_count * sizeof (owl_u32);

    vkCmdCopyBuffer (r->immidiate_command_buffer, iref.buffer,
                     model->indices_buffer, 1, &copy);
  }

  code = owl_renderer_immidiate_command_buffer_end (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_immidiate_command_buffer_submit (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  owl_renderer_immidiate_command_buffer_deinit (r);

  owl_renderer_frame_heap_offset_clear (r);

out:
  owl_assert (OWL_SUCCESS == code);
}

owl_private enum owl_code
owl_model_nodes_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                      struct owl_model *model) {
  owl_i32 i;
  struct owl_model_load_state state;
  struct cgltf_scene const *gs;
  enum owl_code code = OWL_SUCCESS;

  gs = gltf->scene;

  code = owl_model_load_state_init (r, gltf, &state);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  for (i = 0; i < OWL_MODEL_MAX_NODE_COUNT; ++i) {
    model->nodes[i].mesh = -1;
    model->nodes[i].parent = -1;
    model->nodes[i].mesh = -1;
    model->nodes[i].skin = -1;
  }

  if (OWL_MODEL_MAX_NODE_COUNT <= (owl_i32)gltf->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out_err_deinit_load_state;
  }

  model->node_count = (owl_i32)gltf->nodes_count;

  for (i = 0; i < (owl_i32)gltf->nodes_count; ++i) {
    code = owl_model_node_load (r, gltf, &gltf->nodes[i], &state, model);

    if (OWL_SUCCESS != code) {
      goto out_err_deinit_load_state;
    }
  }

  if (OWL_MODEL_MAX_NODE_ROOT_COUNT <= (owl_i32)gs->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out_err_deinit_load_state;
  }

  model->root_count = (owl_i32)gs->nodes_count;

  for (i = 0; i < (owl_i32)gs->nodes_count; ++i) {
    model->roots[i] = (owl_model_node_id)(gs->nodes[i] - gltf->nodes);
  }

  owl_model_buffers_load (r, &state, model);

out_err_deinit_load_state:
  owl_model_load_state_deinit (r, &state);

out:
  return code;
}

owl_private enum owl_code
owl_model_skins_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                      struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  if (OWL_MODEL_MAX_SKIN_COUNT <= (owl_i32)gltf->skins_count) {
    owl_assert (0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->skin_count = (owl_i32)gltf->skins_count;

  for (i = 0; i < (owl_i32)gltf->skins_count; ++i) {
    owl_i32 j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *gs = &gltf->skins[i];
    struct owl_model_skin *skin = &model->skins[i];

    if (gs->name) {
      owl_strncpy (skin->name, gs->name, OWL_MODEL_MAX_NAME_LENGTH);
    } else {
      owl_strncpy (skin->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);
    }

    skin->skeleton_root = (owl_model_node_id)(gs->skeleton - gltf->nodes);

    if (OWL_MODEL_SKIN_MAX_JOINT_COUNT <= (owl_i32)gs->joints_count) {
      owl_assert (0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    skin->joint_count = (owl_i32)gs->joints_count;

    for (j = 0; j < (owl_i32)gs->joints_count; ++j) {
      skin->joints[j] = (owl_model_node_id)(gs->joints[j] - gltf->nodes);

      owl_assert (!owl_strncmp (model->nodes[skin->joints[j]].name,
                                gs->joints[j]->name,
                                OWL_MODEL_MAX_NAME_LENGTH));
    }

    inverse_bind_matrices =
        owl_resolve_gltf_accessor (gs->inverse_bind_matrices);

    for (j = 0; j < (owl_i32)gs->inverse_bind_matrices->count; ++j) {
      owl_m4_copy (inverse_bind_matrices[j], skin->inverse_bind_matrices[j]);
    }

    {
      VkBufferCreateInfo info;

      skin->ssbo_buffer_size = sizeof (struct owl_model_skin_ssbo);

      info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.size = skin->ssbo_buffer_size;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        OWL_VK_CHECK (
            vkCreateBuffer (r->device, &info, NULL, &skin->ssbo_buffers[j]));
      }
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo info;

      vkGetBufferMemoryRequirements (r->device, skin->ssbo_buffers[0],
                                     &requirements);

      skin->ssbo_buffer_alignment = requirements.alignment;
      skin->ssbo_buffer_aligned_size =
          owl_alignu2 (skin->ssbo_buffer_size, requirements.alignment);

      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = NULL;
      info.allocationSize =
          skin->ssbo_buffer_aligned_size * OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
      info.memoryTypeIndex = owl_renderer_find_memory_type (
          r, requirements.memoryTypeBits,
          OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

      OWL_VK_CHECK (
          vkAllocateMemory (r->device, &info, NULL, &skin->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        OWL_VK_CHECK (vkBindBufferMemory (
            r->device, skin->ssbo_buffers[j], skin->ssbo_memory,
            (owl_u64)j * skin->ssbo_buffer_aligned_size));
      }
    }

    {
      VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
      VkDescriptorSetAllocateInfo info;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        layouts[j] = r->vertex_ssbo_set_layout;
      }

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = r->common_set_pool;
      info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
      info.pSetLayouts = layouts;

      OWL_VK_CHECK (
          vkAllocateDescriptorSets (r->device, &info, skin->ssbo_sets));
    }

    {
      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
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

        vkUpdateDescriptorSets (r->device, 1, &write, 0, NULL);
      }
    }

    {
      owl_i32 k;
      void *data;

      vkMapMemory (r->device, skin->ssbo_memory, 0, VK_WHOLE_SIZE, 0, &data);

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        owl_u64 offset = (owl_u64)j * skin->ssbo_buffer_aligned_size;
        owl_byte *ssbo = &((owl_byte *)data)[offset];
        skin->ssbo_datas[j] = (struct owl_model_skin_ssbo *)ssbo;
      }

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
        struct owl_model_skin_ssbo *ssbo = skin->ssbo_datas[j];

        owl_m4_identity (ssbo->matrix);

        for (k = 0; k < (owl_i32)gs->inverse_bind_matrices->count; ++k) {
          owl_m4_copy (inverse_bind_matrices[k], ssbo->joint_matices[k]);
        }

        ssbo->joint_matrice_count = skin->joint_count;
      }
    }
  }
out:
  return code;
}

owl_private enum owl_code
owl_model_anims_load (struct owl_renderer *r, struct cgltf_data const *gltf,
                      struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  owl_unused (r);

  if (OWL_MODEL_MAX_ANIM_COUNT <= (owl_i32)gltf->animations_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  model->anim_count = (owl_i32)gltf->animations_count;

  for (i = 0; i < (owl_i32)gltf->animations_count; ++i) {
    owl_i32 j;
    struct cgltf_animation const *ga;
    struct owl_model_anim *anim;

    ga = &gltf->animations[i];
    anim = &model->anims[i];

    anim->current_time = 0.0F;

    if (OWL_MODEL_ANIM_MAX_SAMPLER_COUNT <= (owl_i32)ga->samplers_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    anim->sampler_count = (owl_i32)ga->samplers_count;

    anim->begin = FLT_MAX;
    anim->end = FLT_MIN;

    for (j = 0; j < (owl_i32)ga->samplers_count; ++j) {
      owl_i32 k;
      float const *inputs;
      struct cgltf_animation_sampler const *gs;
      owl_model_anim_sampler_id sid;
      struct owl_model_anim_sampler *sampler;

      gs = &ga->samplers[j];

      sid = model->anim_sampler_count++;

      if (OWL_MODEL_MAX_SAMPLER_COUNT <= sid) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler = &model->anim_samplers[sid];

      if (OWL_MODEL_ANIM_SAMPLER_MAX_INPUT_COUNT <=
          (owl_i32)gs->input->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler->input_count = (owl_i32)gs->input->count;

      inputs = owl_resolve_gltf_accessor (gs->input);

      for (k = 0; k < (owl_i32)gs->input->count; ++k) {
        float const input = inputs[k];

        sampler->inputs[k] = input;

        if (input < anim->begin) {
          anim->begin = input;
        }

        if (input > anim->end) {
          anim->end = input;
        }
      }

      if (OWL_MODEL_ANIM_SAMPLER_MAX_OUTPUT_COUNT <=
          (owl_i32)gs->output->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      sampler->output_count = (owl_i32)gs->output->count;

      switch (gs->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor (gs->output);

        for (k = 0; k < (owl_i32)gs->output->count; ++k) {
          owl_v4_zero (sampler->outputs[k]);
          owl_v3_copy (outputs[k], sampler->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor (gs->output);
        for (k = 0; k < (owl_i32)gs->output->count; ++k) {
          owl_v4_copy (outputs[k], sampler->outputs[k]);
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

      sampler->interpolation = (owl_i32)gs->interpolation;

      anim->samplers[j] = sid;
    }

    if (OWL_MODEL_ANIM_MAX_CHAN_COUNT <= (owl_i32)ga->channels_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    anim->chan_count = (owl_i32)ga->channels_count;

    for (j = 0; j < (owl_i32)ga->channels_count; ++j) {
      struct cgltf_animation_channel const *gc;
      owl_model_anim_chan_id cid;
      struct owl_model_anim_chan *chan;

      gc = &ga->channels[j];

      cid = model->anim_chan_count++;

      if (OWL_MODEL_MAX_CHAN_COUNT <= cid) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto out;
      }

      chan = &model->anim_chans[cid];

      chan->path = (owl_i32)gc->target_path;
      chan->node = (owl_model_anim_chan_id)(gc->target_node - gltf->nodes);
      chan->anim_sampler = anim->samplers[(gc->sampler - ga->samplers)];

      anim->chans[j] = cid;
    }
  }

out:
  return code;
}

owl_public enum owl_code
owl_model_init (struct owl_model *model, struct owl_renderer *r,
                char const *path) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;

  enum owl_code code = OWL_SUCCESS;

  owl_assert (owl_renderer_frame_heap_offset_is_clear (r));

  owl_memset (&options, 0, sizeof (options));
  owl_memset (model, 0, sizeof (*model));

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

  if (cgltf_result_success != cgltf_parse_file (&options, path, &data)) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (cgltf_result_success != cgltf_load_buffers (&options, data, path)) {
    code = OWL_ERROR_UNKNOWN;
    goto out_err_data_free;
  }

  code = owl_model_images_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

  code = owl_model_textures_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

  code = owl_model_materials_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

  code = owl_model_nodes_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

  code = owl_model_skins_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

  code = owl_model_anims_load (r, data, model);
  if (OWL_SUCCESS != code) {
    goto out_err_data_free;
  }

out_err_data_free:
  cgltf_free (data);

  owl_renderer_frame_heap_offset_clear (r);

out:
  return code;
}

void
owl_model_deinit (struct owl_model *model, struct owl_renderer *r) {
  owl_i32 i;

  OWL_VK_CHECK (vkDeviceWaitIdle (r->device));

  vkFreeMemory (r->device, model->indices_memory, NULL);
  vkDestroyBuffer (r->device, model->indices_buffer, NULL);
  vkFreeMemory (r->device, model->vertices_memory, NULL);
  vkDestroyBuffer (r->device, model->vertices_buffer, NULL);

  for (i = 0; i < model->image_count; ++i) {
    owl_renderer_image_deinit (r, model->images[i].renderer_image);
  }

  for (i = 0; i < model->skin_count; ++i) {
    owl_i32 j;

    vkFreeDescriptorSets (r->device, r->common_set_pool,
                          OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                          model->skins[i].ssbo_sets);

    vkFreeMemory (r->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++j) {
      vkDestroyBuffer (r->device, model->skins[i].ssbo_buffers[j], NULL);
    }
  }
}

owl_private void
owl_model_resolve_local_node_matrix (struct owl_model const *model,
                                     owl_model_node_id nid, owl_m4 matrix) {
  owl_m4 tmp;
  struct owl_model_node const *node;

  node = &model->nodes[nid];

  owl_m4_identity (matrix);
  owl_m4_translate (node->translation, matrix);

  owl_m4_identity (tmp);
  owl_q4_as_m4 (node->rotation, tmp);
  owl_m4_multiply (matrix, tmp, matrix);

  owl_m4_identity (tmp);
  owl_m4_scale_v3 (tmp, node->scale, tmp);
  owl_m4_multiply (matrix, tmp, matrix);

  owl_m4_multiply (matrix, node->matrix, matrix);
}

owl_private void
owl_model_resolve_node_matrix (struct owl_model const *model,
                               owl_model_node_id nid, owl_m4 matrix) {
  owl_model_node_id parent;

  owl_model_resolve_local_node_matrix (model, nid, matrix);

  for (parent = model->nodes[nid].parent; OWL_MODEL_NODE_PARENT_NONE != parent;
       parent = model->nodes[parent].parent) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix (model, parent, local);
    owl_m4_multiply (local, matrix, matrix);
  }
}

owl_private void
owl_model_node_joints_update (struct owl_model *model, owl_i32 frame,
                              owl_model_node_id nid) {
  owl_i32 i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin const *skin;
  struct owl_model_node const *node;

  node = &model->nodes[nid];

  for (i = 0; i < node->child_count; ++i) {
    owl_model_node_joints_update (model, frame, node->children[i]);
  }

  if (OWL_MODEL_SKIN_NONE == node->skin) {
    goto out;
  }

  skin = &model->skins[node->skin];

  owl_model_resolve_node_matrix (model, nid, tmp);
  owl_m4_inverse (tmp, inverse);

  for (i = 0; i < skin->joint_count; ++i) {
    struct owl_model_skin_ssbo *ssbo = skin->ssbo_datas[frame];

    owl_model_resolve_node_matrix (model, skin->joints[i], tmp);
    owl_m4_multiply (tmp, skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply (inverse, tmp, ssbo->joint_matices[i]);
  }

out:
  return;
}

#define OWL_MODEL_ANIM_INTERPOLATION_TYPE_LINEAR                              \
  cgltf_interpolation_type_linear
#define OWL_MODEL_ANIM_PATH_TYPE_TRANSLATION                                  \
  cgltf_animation_path_type_translation
#define OWL_MODEL_ANIM_PATH_TYPE_ROTATION cgltf_animation_path_type_rotation
#define OWL_MODEL_ANIM_PATH_TYPE_SCALE    cgltf_animation_path_type_scale

owl_public enum owl_code
owl_model_anim_update (struct owl_model *model, owl_i32 frame, float dt,
                       owl_model_anim_id id) {
  owl_i32 i;
  struct owl_model_anim *anim;

  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_ANIM_NONE == id) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  anim = &model->anims[id];

  if (anim->end < (anim->current_time += dt)) {
    anim->current_time -= anim->end;
  }

  for (i = 0; i < anim->chan_count; ++i) {
    owl_i32 j;
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
      OWL_DEBUG_LOG ("skipping channel %i\n", i);
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
        owl_v3_mix (sampler->outputs[j], sampler->outputs[j + 1], a,
                    node->translation);
      } break;

      case OWL_MODEL_ANIM_PATH_TYPE_ROTATION: {
        owl_v4_quat_slerp (sampler->outputs[j], sampler->outputs[j + 1], a,
                           node->rotation);
        owl_v4_normalize (node->rotation, node->rotation);
      } break;

      case OWL_MODEL_ANIM_PATH_TYPE_SCALE: {
        owl_v3_mix (sampler->outputs[j], sampler->outputs[j + 1], a,
                    node->scale);
      } break;

      default:
        owl_assert (0 && "unexpected path");
        code = OWL_ERROR_UNKNOWN;
        goto out;
      }
    }
  }

  for (i = 0; i < model->root_count; ++i) {
    owl_model_node_joints_update (model, frame, model->roots[i]);
  }

out:
  return code;
}

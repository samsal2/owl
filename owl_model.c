#include "owl_model.h"

#include "cgltf.h"
#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_types.h"
#include "owl_vector_math.h"

#include <float.h>
#include <stdio.h>

struct owl_model_uri {
  char path[OWL_MODEL_MAX_NAME_LENGTH];
};

OWL_INTERNAL void const *
owl_resolve_gltf_accessor_(struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  owl_byte const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

OWL_INTERNAL enum owl_code owl_model_uri_init_(char const *src,
                                               struct owl_model_uri *uri) {
  enum owl_code code = OWL_SUCCESS;

  snprintf(uri->path, OWL_MODEL_MAX_NAME_LENGTH, "../assets/%s", src);

  return code;
}

OWL_INTERNAL enum owl_code owl_model_load_images_(struct owl_renderer *renderer,
                                                  struct cgltf_data const *gltf,
                                                  struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)gltf->images_count; ++i) {
    struct owl_model_uri uri;
    struct owl_renderer_image_init_desc image_init_desc;
    struct owl_model_image_data *dst_image = &model->images[i];

    if (OWL_SUCCESS != (code = owl_model_uri_init_(gltf->images[i].uri, &uri)))
      goto end;

    image_init_desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_FILE;
    image_init_desc.src_path = uri.path;
    /* FIXME(samuel): if im not mistaken, gltf defines some sampler requirements
     * . Completely ignoring it for now */
    image_init_desc.use_default_sampler = 1;

    code =
        owl_renderer_init_image(renderer, &image_init_desc, &dst_image->image);

    if (OWL_SUCCESS != code)
      goto end;
  }

  model->images_count = (owl_i32)gltf->images_count;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_textures_(struct owl_renderer *renderer,
                         struct cgltf_data const *gltf,
                         struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  model->textures_count = (owl_i32)gltf->textures_count;

  for (i = 0; i < (owl_i32)gltf->textures_count; ++i) {
    struct cgltf_texture const *src_texture = &gltf->textures[i];
    model->textures[i].image.slot =
        (owl_i32)(src_texture->image - gltf->images);
  }

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_materials_(struct owl_renderer *renderer,
                          struct cgltf_data const *gltf,
                          struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  if (OWL_MODEL_MAX_MATERIALS_COUNT <= (owl_i32)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->materials_count = (owl_i32)gltf->materials_count;

  for (i = 0; i < (owl_i32)gltf->materials_count; ++i) {
    struct cgltf_material const *src_material = &gltf->materials[i];
    struct owl_model_material_data *dst_material = &model->materials[i];

    OWL_ASSERT(src_material->has_pbr_metallic_roughness);

    OWL_V4_COPY(src_material->pbr_metallic_roughness.base_color_factor,
                dst_material->base_color_factor);

    dst_material->base_color_texture.slot =
        (owl_i32)(src_material->pbr_metallic_roughness.base_color_texture
                      .texture -
                  gltf->textures);

#if 0
    OWL_ASSERT(src_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (owl_i32)(src_material->normal_texture.texture - gltf->textures);
#else
    /* HACK(samuel): currently we don't check for the normal texture */
    dst_material->normal_texture.slot = dst_material->base_color_texture.slot;
#endif

    /* FIXME(samuel): make sure this is right */
    dst_material->alpha_mode = (enum owl_alpha_mode)src_material->alpha_mode;
    dst_material->alpha_cutoff = src_material->alpha_cutoff;
    dst_material->double_sided = src_material->double_sided;
  }

end:
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
owl_find_gltf_attribute_(struct cgltf_primitive const *src_primitive,
                         char const *attribute_name) {
  owl_i32 i;
  struct cgltf_attribute const *attribute = NULL;

  for (i = 0; i < (owl_i32)src_primitive->attributes_count; ++i) {
    struct cgltf_attribute const *current = &src_primitive->attributes[i];

    if (!OWL_STRNCMP(current->name, attribute_name,
                     OWL_MODEL_MAX_NAME_LENGTH)) {
      attribute = current;
      goto end;
    }
  }

end:
  return attribute;
}

OWL_INTERNAL void
owl_model_find_load_desc_capacities_(struct cgltf_data const *gltf,
                                     struct owl_model_load_state *state) {
  owl_i32 i;
  for (i = 0; i < (owl_i32)gltf->nodes_count; ++i) {
    owl_i32 j;
    struct cgltf_node const *src_node = &gltf->nodes[i];

    if (!src_node->mesh)
      continue;

    for (j = 0; j < (owl_i32)src_node->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attribute;
      struct cgltf_primitive const *primitive = &src_node->mesh->primitives[j];

      attribute = owl_find_gltf_attribute_(primitive, "POSITION");
      state->vertices_capacity += attribute->data->count;
      state->indices_capacity += primitive->indices->count;
    }
  }
}

OWL_INTERNAL enum owl_code
owl_model_init_load_state_(struct owl_renderer *renderer,
                           struct cgltf_data const *gltf,
                           struct owl_model_load_state *state) {
  owl_u64 size;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  state->vertices_capacity = 0;
  state->indices_capacity = 0;
  state->vertices_count = 0;
  state->indices_count = 0;

  owl_model_find_load_desc_capacities_(gltf, state);

  size = (owl_u64)state->vertices_capacity * sizeof(*state->vertices);
  if (!(state->vertices = OWL_MALLOC(size))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  size = (owl_u64)state->indices_capacity * sizeof(owl_u32);
  if (!(state->indices = OWL_MALLOC(size))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_err_free_vertices;
  }

  goto end;

end_err_free_vertices:
  OWL_FREE(state->vertices);

end:
  return code;
}

OWL_INTERNAL void
owl_model_deinit_load_state_(struct owl_renderer *renderer,
                             struct owl_model_load_state *state) {
  OWL_UNUSED(renderer);

  OWL_FREE(state->indices);
  OWL_FREE(state->vertices);
}

OWL_INTERNAL enum owl_code owl_model_load_node_(
    struct owl_renderer *renderer, struct cgltf_data const *gltf,
    struct cgltf_node const *src_node, struct owl_model_load_state *state,
    struct owl_model *model) {

  owl_i32 i;
  struct owl_model_node node;
  struct owl_model_node_data *dst_node;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  node.slot = (owl_i32)(src_node - gltf->nodes);

  if (OWL_MODEL_MAX_NODES_COUNT <= node.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  dst_node = &model->nodes[node.slot];

  if (src_node->parent)
    dst_node->parent.slot = (owl_i32)(src_node->parent - gltf->nodes);
  else
    dst_node->parent.slot = OWL_MODEL_NODE_NO_PARENT_SLOT;

  if (OWL_MODEL_NODE_MAX_CHILDREN_COUNT <= (owl_i32)src_node->children_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  dst_node->children_count = (owl_i32)src_node->children_count;

  for (i = 0; i < (owl_i32)src_node->children_count; ++i)
    dst_node->children[i].slot = (owl_i32)(src_node->children[i] - gltf->nodes);

  if (src_node->name)
    OWL_STRNCPY(dst_node->name, src_node->name, OWL_MODEL_MAX_NAME_LENGTH);
  else
    OWL_STRNCPY(dst_node->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);

  if (src_node->has_translation)
    OWL_V3_COPY(src_node->translation, dst_node->translation);
  else
    OWL_V3_ZERO(dst_node->translation);

  if (src_node->has_rotation)
    OWL_V4_COPY(src_node->rotation, dst_node->rotation);
  else
    OWL_V4_ZERO(dst_node->rotation);

  if (src_node->has_scale)
    OWL_V3_COPY(src_node->scale, dst_node->scale);
  else
    OWL_V3_SET(1.0F, 1.0F, 1.0F, dst_node->scale);

  if (src_node->has_matrix)
    OWL_M4_COPY_V16(src_node->matrix, dst_node->matrix);
  else
    OWL_M4_IDENTITY(dst_node->matrix);

  if (src_node->skin)
    dst_node->skin.slot = (owl_i32)(src_node->skin - gltf->skins);
  else
    dst_node->skin.slot = OWL_MODEL_NODE_NO_SKIN_SLOT;

  if (src_node->mesh) {
    struct cgltf_mesh const *src_mesh;
    struct owl_model_mesh_data *dst_mesh;

    dst_node->mesh.slot = model->meshes_count++;

    if (OWL_MODEL_MAX_MESHES_COUNT <= dst_node->mesh.slot) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    src_mesh = src_node->mesh;
    dst_mesh = &model->meshes[dst_node->mesh.slot];

    dst_mesh->primitives_count = (owl_i32)src_mesh->primitives_count;

    for (i = 0; i < (owl_i32)src_mesh->primitives_count; ++i) {
      owl_i32 j;
      owl_i32 vertices_count = 0;
      owl_i32 indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv0 = NULL;
      float const *uv1 = NULL;
      owl_u16 const *joints0 = NULL;
      float const *weights0 = NULL;
      struct cgltf_attribute const *attribute = NULL;
      struct owl_model_primitive *primitive = NULL;
      struct owl_model_primitive_data *dst_primitive = NULL;
      struct cgltf_primitive const *src_primitive = &src_mesh->primitives[i];

      primitive = &dst_mesh->primitives[i];
      primitive->slot = model->primitives_count++;

      if (OWL_MODEL_MAX_PRIMITIVES_COUNT <= primitive->slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_primitive = &model->primitives[primitive->slot];

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "POSITION"))) {
        position = owl_resolve_gltf_accessor_(attribute->data);
        vertices_count = (owl_i32)attribute->data->count;
      }

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "NORMAL")))
        normal = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "TEXCOORD_0")))
        uv0 = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "TEXCOORD_1")))
        uv1 = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "JOINTS_0")))
        joints0 = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "WEIGHTS_0")))
        weights0 = owl_resolve_gltf_accessor_(attribute->data);

      for (j = 0; j < vertices_count; ++j) {
        owl_i32 offset = state->vertices_count;
        struct owl_model_vertex *vertex = &state->vertices[offset + j];

        OWL_V3_COPY(&position[j * 3], vertex->position);

        if (normal)
          OWL_V3_COPY(&normal[j * 3], vertex->normal);
        else
          OWL_V3_ZERO(vertex->normal);

        if (uv0)
          OWL_V2_COPY(&uv0[j * 2], vertex->uv0);
        else
          OWL_V2_ZERO(vertex->uv0);

        if (uv1)
          OWL_V2_COPY(&uv1[j * 2], vertex->uv1);
        else
          OWL_V2_ZERO(vertex->uv1);

        if (joints0 && weights0)
          OWL_V4_COPY(&joints0[j * 4], vertex->joints0);
        else
          OWL_V4_ZERO(vertex->joints0);

        if (joints0 && weights0)
          OWL_V4_COPY(&weights0[j * 4], vertex->weights0);
        else
          OWL_V4_ZERO(vertex->weights0);
      }

      indices_count = (owl_i32)src_primitive->indices->count;

      switch (src_primitive->indices->component_type) {
      case cgltf_component_type_r_32u: {
        owl_u32 const *indices;
        owl_i32 offset = state->indices_count;
        indices = owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (owl_i32)src_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u32)state->vertices_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        owl_u16 const *indices;
        owl_i32 offset = state->indices_count;
        indices = owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (owl_i32)src_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u16)state->vertices_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        owl_u8 const *indices;
        owl_i32 offset = state->indices_count;
        indices = owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (owl_i32)src_primitive->indices->count; ++j) {
          state->indices[offset + j] =
              indices[j] + (owl_u8)state->vertices_count;
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_UNKNOWN;
        goto end;
      }

      dst_primitive->first = (owl_u32)state->indices_count;
      dst_primitive->count = (owl_u32)indices_count;
      dst_primitive->material.slot =
          (owl_i32)(src_primitive->material - gltf->materials);

      state->indices_count += indices_count;
      state->vertices_count += vertices_count;
    }
  }

end:
  return code;
}

OWL_INTERNAL void
owl_model_load_buffers_(struct owl_renderer *renderer,
                        struct owl_model_load_state const *desc,
                        struct owl_model *model) {
  struct owl_renderer_dynamic_heap_reference vertex_reference;
  struct owl_renderer_dynamic_heap_reference index_reference;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(renderer));
  OWL_ASSERT(desc->vertices_count == desc->vertices_capacity);
  OWL_ASSERT(desc->indices_count == desc->indices_capacity);

  {
    owl_u64 size;

    size = (owl_u64)desc->vertices_capacity * sizeof(struct owl_model_vertex);
    owl_renderer_dynamic_heap_submit(renderer, size, desc->vertices,
                                     &vertex_reference);

    size = (owl_u64)desc->indices_capacity * sizeof(owl_u32);
    owl_renderer_dynamic_heap_submit(renderer, size, desc->indices,
                                     &index_reference);
  }

  {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size =
        (owl_u64)desc->vertices_count * sizeof(struct owl_model_vertex);
    buffer_create_info.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                                &model->vertices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetBufferMemoryRequirements(renderer->device, model->vertices_buffer,
                                  &requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                  &model->vertices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(renderer->device, model->vertices_buffer,
                                    model->vertices_memory, 0));
  }

  {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = (owl_u64)desc->indices_count * sizeof(owl_u32);
    buffer_create_info.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                                &model->indices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetBufferMemoryRequirements(renderer->device, model->indices_buffer,
                                  &requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                  &model->indices_memory));

    OWL_VK_CHECK(vkBindBufferMemory(renderer->device, model->indices_buffer,
                                    model->indices_memory, 0));
  }

  code = owl_renderer_begin_immidiate_command_buffer(renderer);

  if (OWL_SUCCESS != code)
    goto end;

  {
    VkBufferCopy copy;

    copy.srcOffset = vertex_reference.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)desc->vertices_count * sizeof(struct owl_model_vertex);

    vkCmdCopyBuffer(renderer->immidiate_command_buffer, vertex_reference.buffer,
                    model->vertices_buffer, 1, &copy);
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = index_reference.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)desc->indices_count * sizeof(owl_u32);

    vkCmdCopyBuffer(renderer->immidiate_command_buffer, index_reference.buffer,
                    model->indices_buffer, 1, &copy);
  }

  if (OWL_SUCCESS !=
      (code = owl_renderer_end_immidiate_command_buffer(renderer)))
    goto end;

  owl_renderer_clear_dynamic_heap_offset(renderer);

end:
  OWL_ASSERT(OWL_SUCCESS == code);
  return;
}

OWL_INTERNAL enum owl_code owl_model_load_nodes_(struct owl_renderer *renderer,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  owl_i32 i;
  struct owl_model_load_state load_state;
  struct cgltf_scene const *src_scene;
  enum owl_code code = OWL_SUCCESS;

  src_scene = gltf->scene;

  if (OWL_SUCCESS !=
      (code = owl_model_init_load_state_(renderer, gltf, &load_state)))
    goto end;

  for (i = 0; i < OWL_MODEL_MAX_NODES_COUNT; ++i) {
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].parent.slot = -1;
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].skin.slot = -1;
  }

  if (OWL_MODEL_MAX_NODES_COUNT <= (owl_i32)gltf->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_state;
  }

  model->nodes_count = (owl_i32)gltf->nodes_count;

  for (i = 0; i < (owl_i32)gltf->nodes_count; ++i) {
    code = owl_model_load_node_(renderer, gltf, &gltf->nodes[i], &load_state,
                                model);

    if (OWL_SUCCESS != code)
      goto end_err_deinit_load_state;
  }

  if (OWL_MODEL_MAX_NODE_ROOTS_COUNT <= (owl_i32)src_scene->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_state;
  }

  model->roots_count = (owl_i32)src_scene->nodes_count;

  for (i = 0; i < (owl_i32)src_scene->nodes_count; ++i)
    model->roots[i].slot = (owl_i32)(src_scene->nodes[i] - gltf->nodes);

  owl_model_load_buffers_(renderer, &load_state, model);

end_err_deinit_load_state:
  owl_model_deinit_load_state_(renderer, &load_state);

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_model_load_skins_(struct owl_renderer *renderer,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  if (OWL_MODEL_MAX_SKINS_COUNT <= (owl_i32)gltf->skins_count) {
    OWL_ASSERT(0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->skins_count = (owl_i32)gltf->skins_count;

  for (i = 0; i < (owl_i32)gltf->skins_count; ++i) {
    owl_i32 j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *src_skin = &gltf->skins[i];
    struct owl_model_skin_data *dst_skin = &model->skins[i];

    if (src_skin->name)
      OWL_STRNCPY(dst_skin->name, src_skin->name, OWL_MODEL_MAX_NAME_LENGTH);
    else
      OWL_STRNCPY(dst_skin->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);

    dst_skin->skeleton_root.slot = (owl_i32)(src_skin->skeleton - gltf->nodes);

    if (OWL_MODEL_SKIN_MAX_JOINTS_COUNT <= (owl_i32)src_skin->joints_count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_skin->joints_count = (owl_i32)src_skin->joints_count;

    for (j = 0; j < (owl_i32)src_skin->joints_count; ++j) {

      dst_skin->joints[j].slot = (owl_i32)(src_skin->joints[j] - gltf->nodes);

      OWL_ASSERT(!OWL_STRNCMP(model->nodes[dst_skin->joints[j].slot].name,
                              src_skin->joints[j]->name,
                              OWL_MODEL_MAX_NAME_LENGTH));
    }

    inverse_bind_matrices =
        owl_resolve_gltf_accessor_(src_skin->inverse_bind_matrices);

    for (j = 0; j < (owl_i32)src_skin->inverse_bind_matrices->count; ++j)
      OWL_M4_COPY(inverse_bind_matrices[j], dst_skin->inverse_bind_matrices[j]);

    {
      VkBufferCreateInfo buffer_create_info;

      dst_skin->ssbo_buffer_size = sizeof(struct owl_model_skin_ssbo_data);

      buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer_create_info.pNext = NULL;
      buffer_create_info.flags = 0;
      buffer_create_info.size = dst_skin->ssbo_buffer_size;
      buffer_create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      buffer_create_info.queueFamilyIndexCount = 0;
      buffer_create_info.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                                    &dst_skin->ssbo_buffers[j]));
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo memory_allocate_info;

      vkGetBufferMemoryRequirements(renderer->device, dst_skin->ssbo_buffers[0],
                                    &requirements);

      dst_skin->ssbo_buffer_alignment = requirements.alignment;
      dst_skin->ssbo_buffer_aligned_size =
          OWL_ALIGNU2(dst_skin->ssbo_buffer_size, requirements.alignment);

      memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memory_allocate_info.pNext = NULL;
      memory_allocate_info.allocationSize = dst_skin->ssbo_buffer_aligned_size *
                                            OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
          renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

      OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info,
                                    NULL, &dst_skin->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkBindBufferMemory(
            renderer->device, dst_skin->ssbo_buffers[j], dst_skin->ssbo_memory,
            (owl_u64)j * dst_skin->ssbo_buffer_aligned_size));
    }

    {
      VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
      VkDescriptorSetAllocateInfo set_allocate_info;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        layouts[j] = renderer->vertex_ssbo_set_layout;

      set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      set_allocate_info.pNext = NULL;
      set_allocate_info.descriptorPool = renderer->common_set_pool;
      set_allocate_info.descriptorSetCount =
          OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      set_allocate_info.pSetLayouts = layouts;

      OWL_VK_CHECK(vkAllocateDescriptorSets(
          renderer->device, &set_allocate_info, dst_skin->ssbo_sets));
    }

    {

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        VkDescriptorBufferInfo buffer_descriptor;
        VkWriteDescriptorSet write;

        buffer_descriptor.buffer = dst_skin->ssbo_buffers[j];
        buffer_descriptor.offset = 0;
        buffer_descriptor.range = dst_skin->ssbo_buffer_size;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = NULL;
        write.dstSet = dst_skin->ssbo_sets[j];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pImageInfo = NULL;
        write.pBufferInfo = &buffer_descriptor;
        write.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
      }
    }

    {
      owl_i32 k;
      void *data;

      vkMapMemory(renderer->device, dst_skin->ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data);

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        owl_u64 offset = (owl_u64)j * dst_skin->ssbo_buffer_aligned_size;
        owl_byte *ssbo = &((owl_byte *)data)[offset];
        dst_skin->ssbo_datas[j] = (struct owl_model_skin_ssbo_data *)ssbo;
      }

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        struct owl_model_skin_ssbo_data *ssbo = dst_skin->ssbo_datas[j];

        OWL_M4_IDENTITY(ssbo->matrix);

        for (k = 0; k < (owl_i32)src_skin->inverse_bind_matrices->count; ++k)
          OWL_M4_COPY(inverse_bind_matrices[k], ssbo->joint_matices[k]);

        ssbo->joint_matrices_count = dst_skin->joints_count;
      }
    }
  }
end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_animations_(struct owl_renderer *renderer,
                           struct cgltf_data const *gltf,
                           struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(renderer);

  if (OWL_MODEL_MAX_ANIMATIONS_COUNT <= (owl_i32)gltf->animations_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->animations_count = (owl_i32)gltf->animations_count;

  for (i = 0; i < (owl_i32)gltf->animations_count; ++i) {
    owl_i32 j;
    struct cgltf_animation const *src_animation;
    struct owl_model_animation_data *dst_animation;

    src_animation = &gltf->animations[i];
    dst_animation = &model->animations[i];

    dst_animation->current_time = 0.0F;

    if (OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT <=
        (owl_i32)src_animation->samplers_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_animation->samplers_count = (owl_i32)src_animation->samplers_count;

    dst_animation->begin = FLT_MAX;
    dst_animation->end = FLT_MIN;

    for (j = 0; j < (owl_i32)src_animation->samplers_count; ++j) {
      owl_i32 k;
      float const *inputs;
      struct cgltf_animation_sampler const *src_sampler;
      struct owl_model_animation_sampler sampler;
      struct owl_model_animation_sampler_data *dst_sampler;

      src_sampler = &src_animation->samplers[j];

      sampler.slot = model->animation_samplers_count++;

      if (OWL_MODEL_MAX_SAMPLERS_COUNT <= sampler.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_sampler = &model->animation_samplers[sampler.slot];

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_INPUTS_COUNT <=
          (owl_i32)src_sampler->input->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_sampler->inputs_count = (owl_i32)src_sampler->input->count;

      inputs = owl_resolve_gltf_accessor_(src_sampler->input);

      for (k = 0; k < (owl_i32)src_sampler->input->count; ++k) {
        float const input = inputs[k];

        dst_sampler->inputs[k] = input;

        if (input < dst_animation->begin)
          dst_animation->begin = input;

        if (input > dst_animation->end)
          dst_animation->end = input;
      }

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT <=
          (owl_i32)src_sampler->output->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_sampler->outputs_count = (owl_i32)src_sampler->output->count;

      switch (src_sampler->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor_(src_sampler->output);

        for (k = 0; k < (owl_i32)src_sampler->output->count; ++k) {
          OWL_V4_ZERO(dst_sampler->outputs[k]);
          OWL_V3_COPY(outputs[k], dst_sampler->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor_(src_sampler->output);
        for (k = 0; k < (owl_i32)src_sampler->output->count; ++k)
          OWL_V4_COPY(outputs[k], dst_sampler->outputs[k]);
      } break;

      case cgltf_type_invalid:
      case cgltf_type_scalar:
      case cgltf_type_vec2:
      case cgltf_type_mat2:
      case cgltf_type_mat3:
      case cgltf_type_mat4:
      default:
        code = OWL_ERROR_UNKNOWN;
        goto end;
      }

      dst_sampler->interpolation = (owl_i32)src_sampler->interpolation;

      dst_animation->samplers[j].slot = sampler.slot;
    }

    if (OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT <=
        (owl_i32)src_animation->channels_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_animation->channels_count = (owl_i32)src_animation->channels_count;

    for (j = 0; j < (owl_i32)src_animation->channels_count; ++j) {
      struct cgltf_animation_channel const *src_channel;
      struct owl_model_animation_channel channel;
      struct owl_model_animation_channel_data *dst_channel;

      src_channel = &src_animation->channels[j];

      channel.slot = model->animation_channels_count++;

      if (OWL_MODEL_MAX_CHANNELS_COUNT <= channel.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_channel = &model->animation_channels[channel.slot];

      dst_channel->path = (owl_i32)src_channel->target_path;
      dst_channel->node.slot =
          (owl_i32)(src_channel->target_node - gltf->nodes);
      dst_channel->animation_sampler.slot =
          dst_animation
              ->samplers[(src_channel->sampler - src_animation->samplers)]
              .slot;

      dst_animation->channels[j].slot = channel.slot;
    }
  }

end:
  return code;
}

enum owl_code owl_model_init(struct owl_renderer *renderer, char const *path,
                             struct owl_model *model) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(renderer));

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
    goto end;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    code = OWL_ERROR_UNKNOWN;
    goto end_err_free_data;
  }

  if (OWL_SUCCESS != (code = owl_model_load_images_(renderer, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_textures_(renderer, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_materials_(renderer, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_nodes_(renderer, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_skins_(renderer, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_animations_(renderer, data, model)))
    goto end_err_free_data;

end_err_free_data:
  cgltf_free(data);

  owl_renderer_clear_dynamic_heap_offset(renderer);

end:
  return code;
}

void owl_model_deinit(struct owl_renderer *renderer, struct owl_model *model) {
  owl_i32 i;

  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  vkFreeMemory(renderer->device, model->indices_memory, NULL);
  vkDestroyBuffer(renderer->device, model->indices_buffer, NULL);
  vkFreeMemory(renderer->device, model->vertices_memory, NULL);
  vkDestroyBuffer(renderer->device, model->vertices_buffer, NULL);

  for (i = 0; i < model->images_count; ++i)
    owl_renderer_deinit_image(renderer, &model->images[i].image);

  for (i = 0; i < model->skins_count; ++i) {
    owl_i32 j;

    vkFreeDescriptorSets(renderer->device, renderer->common_set_pool,
                         OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                         model->skins[i].ssbo_sets);

    vkFreeMemory(renderer->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
      vkDestroyBuffer(renderer->device, model->skins[i].ssbo_buffers[j], NULL);
  }
}

OWL_INTERNAL void
owl_model_resolve_local_node_matrix_(struct owl_model const *model,
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
owl_model_resolve_node_matrix_(struct owl_model const *model,
                               struct owl_model_node const *node,
                               owl_m4 matrix) {
  struct owl_model_node parent;

  owl_model_resolve_local_node_matrix_(model, node, matrix);

  for (parent.slot = model->nodes[node->slot].parent.slot;
       OWL_MODEL_NODE_NO_PARENT_SLOT != parent.slot;
       parent.slot = model->nodes[parent.slot].parent.slot) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix_(model, &parent, local);
    owl_m4_multiply(local, matrix, matrix);
  }
}

OWL_INTERNAL void
owl_model_node_update_joints_(struct owl_model *model, owl_i32 frame,
                              struct owl_model_node const *node) {
  owl_i32 i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin_data const *dst_skin;
  struct owl_model_node_data const *dst_node;

  dst_node = &model->nodes[node->slot];

  for (i = 0; i < dst_node->children_count; ++i)
    owl_model_node_update_joints_(model, frame, &dst_node->children[i]);

  if (OWL_MODEL_NODE_NO_SKIN_SLOT == dst_node->skin.slot)
    goto end;

  dst_skin = &model->skins[dst_node->skin.slot];

  owl_model_resolve_node_matrix_(model, node, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < dst_skin->joints_count; ++i) {
    struct owl_model_skin_ssbo_data *ssbo = dst_skin->ssbo_datas[frame];

    owl_model_resolve_node_matrix_(model, &dst_skin->joints[i], tmp);
    owl_m4_multiply(tmp, dst_skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp, ssbo->joint_matices[i]);
  }

end:
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
  struct owl_model_animation_data *dst_animation;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_MODEL_NO_ANIMATION_SLOT == animation) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  dst_animation = &model->animations[animation];

  if (dst_animation->end < (dst_animation->current_time += dt))
    dst_animation->current_time -= dst_animation->end;

  for (i = 0; i < dst_animation->channels_count; ++i) {
    owl_i32 j;
    struct owl_model_animation_channel channel;
    struct owl_model_animation_sampler sampler;
    struct owl_model_node_data *dst_node;
    struct owl_model_animation_channel_data const *dst_channel;
    struct owl_model_animation_sampler_data const *dst_sampler;

    channel.slot = dst_animation->channels[i].slot;
    dst_channel = &model->animation_channels[channel.slot];
    sampler.slot = dst_channel->animation_sampler.slot;
    dst_sampler = &model->animation_samplers[sampler.slot];
    dst_node = &model->nodes[dst_channel->node.slot];

    if (OWL_MODEL_ANIMATION_INTERPOLATION_TYPE_LINEAR !=
        dst_sampler->interpolation) {
      OWL_DEBUG_LOG("skipping channel %i\n", i);
      continue;
    }

    for (j = 0; j < dst_sampler->inputs_count - 1; ++j) {
      float const i0 = dst_sampler->inputs[j];
      float const i1 = dst_sampler->inputs[j + 1];
      float const ct = dst_animation->current_time;
      float const a = (ct - i0) / (i1 - i0);

      if (!((ct >= i0) && (ct <= i1)))
        continue;

      switch (dst_channel->path) {
      case OWL_MODEL_ANIMATION_PATH_TYPE_TRANSLATION: {
        owl_v3_mix(dst_sampler->outputs[j], dst_sampler->outputs[j + 1], a,
                   dst_node->translation);
      } break;

      case OWL_MODEL_ANIMATION_PATH_TYPE_ROTATION: {
        owl_v4_quat_slerp(dst_sampler->outputs[j], dst_sampler->outputs[j + 1],
                          a, dst_node->rotation);
        owl_v4_normalize(dst_node->rotation, dst_node->rotation);
      } break;

      case OWL_MODEL_ANIMATION_PATH_TYPE_SCALE: {
        owl_v3_mix(dst_sampler->outputs[j], dst_sampler->outputs[j + 1], a,
                   dst_node->scale);
      } break;

      default:
        OWL_ASSERT(0 && "unexpected path");
        code = OWL_ERROR_UNKNOWN;
        goto end;
      }
    }
  }

  for (i = 0; i < model->roots_count; ++i) {
    owl_model_node_update_joints_(model, frame, &model->roots[i]);
  }

end:
  return code;
}

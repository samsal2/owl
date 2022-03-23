#include "model.h"

#include "internal.h"
#include "renderer.h"
#include "types.h"
#include "vector_math.h"

#include <cgltf/cgltf.h>
#include <float.h>
#include <stdio.h>

#define OWL_MAX_URI_SIZE 128

OWL_INTERNAL void const *
owl_resolve_gltf_accessor_(struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  owl_byte const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

OWL_INTERNAL enum owl_code owl_fix_uri_(char const *uri,
                                        char fixed[OWL_MAX_URI_SIZE]) {
  enum owl_code code = OWL_SUCCESS;

  snprintf(fixed, OWL_MAX_URI_SIZE, "../../assets/%s", uri);

  return code;
}

OWL_INTERNAL enum owl_code owl_model_load_images_(struct owl_renderer *r,
                                                  struct cgltf_data const *gltf,
                                                  struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (int)gltf->images_count; ++i) {
    char uri[OWL_MAX_URI_SIZE];
    struct owl_renderer_image_init_desc riid;
    struct owl_model_image_data *dst_image = &model->images[i];

    if (OWL_SUCCESS != (code = owl_fix_uri_(gltf->images[i].uri, uri)))
      goto end;

    riid.source_type = OWL_RENDERER_IMAGE_SOURCE_TYPE_FILE;
    riid.source_path = uri;
    riid.use_default_sampler = 1;

    code = owl_renderer_init_image(r, &riid, &dst_image->image);

    if (OWL_SUCCESS != code)
      goto end;
  }

  model->images_count = (int)gltf->images_count;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_textures_(struct owl_renderer *r, struct cgltf_data const *gltf,
                         struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  model->textures_count = (int)gltf->textures_count;

  for (i = 0; i < (int)gltf->textures_count; ++i) {
    struct cgltf_texture const *src_texture = &gltf->textures[i];
    model->textures[i].image.slot = (int)(src_texture->image - gltf->images);
  }

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_materials_(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_MATERIALS_COUNT <= (int)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->materials_count = (int)gltf->materials_count;

  for (i = 0; i < (int)gltf->materials_count; ++i) {
    struct cgltf_material const *src_material = &gltf->materials[i];
    struct owl_model_material_data *dst_material = &model->materials[i];

    OWL_ASSERT(src_material->has_pbr_metallic_roughness);

    OWL_V4_COPY(src_material->pbr_metallic_roughness.base_color_factor,
                dst_material->base_color_factor);

    dst_material->base_color_texture.slot =
        (int)(src_material->pbr_metallic_roughness.base_color_texture.texture -
              gltf->textures);

#if 0
    OWL_ASSERT(src_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (int)(src_material->normal_texture.texture - gltf->textures);
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

struct owl_model_load_desc {
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

  for (i = 0; i < (int)src_primitive->attributes_count; ++i) {
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
                                     struct owl_model_load_desc *sli) {
  owl_i32 i;
  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    owl_i32 j;
    struct cgltf_node const *src_node = &gltf->nodes[i];

    if (!src_node->mesh)
      continue;

    for (j = 0; j < (int)src_node->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attribute;
      struct cgltf_primitive const *primitive = &src_node->mesh->primitives[j];

      attribute = owl_find_gltf_attribute_(primitive, "POSITION");
      sli->vertices_capacity += attribute->data->count;
      sli->indices_capacity += primitive->indices->count;
    }
  }
}

OWL_INTERNAL enum owl_code
owl_model_init_load_desc_(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model_load_desc *sli) {
  owl_u64 size;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  sli->vertices_capacity = 0;
  sli->indices_capacity = 0;
  sli->vertices_count = 0;
  sli->indices_count = 0;

  owl_model_find_load_desc_capacities_(gltf, sli);

  size = (owl_u64)sli->vertices_capacity * sizeof(*sli->vertices);
  if (!(sli->vertices = OWL_MALLOC(size))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  size = (owl_u64)sli->indices_capacity * sizeof(owl_u32);
  if (!(sli->indices = OWL_MALLOC(size))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end_err_free_vertices;
  }

  goto end;

end_err_free_vertices:
  OWL_FREE(sli->vertices);

end:
  return code;
}

OWL_INTERNAL void owl_model_deinit_load_desc_(struct owl_renderer *r,
                                              struct owl_model_load_desc *sli) {
  OWL_UNUSED(r);

  OWL_FREE(sli->indices);
  OWL_FREE(sli->vertices);
}

OWL_INTERNAL enum owl_code
owl_model_load_node_(struct owl_renderer *r, struct cgltf_data const *gltf,
                     struct cgltf_node const *src_node,
                     struct owl_model_load_desc *sli, struct owl_model *model) {

  owl_i32 i;
  struct owl_model_node node;
  struct owl_model_node_data *dst_node;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  node.slot = (int)(src_node - gltf->nodes);

  if (OWL_MODEL_MAX_NODES_COUNT <= node.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  dst_node = &model->nodes[node.slot];

  if (src_node->parent)
    dst_node->parent.slot = (int)(src_node->parent - gltf->nodes);
  else
    dst_node->parent.slot = OWL_MODEL_NODE_NO_PARENT_SLOT;

  if (OWL_MODEL_NODE_MAX_CHILDREN_COUNT <= (int)src_node->children_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  dst_node->children_count = (int)src_node->children_count;

  for (i = 0; i < (int)src_node->children_count; ++i)
    dst_node->children[i].slot = (int)(src_node->children[i] - gltf->nodes);

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
    dst_node->skin.slot = (int)(src_node->skin - gltf->skins);
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

    dst_mesh->primitives_count = (int)src_mesh->primitives_count;

    for (i = 0; i < (int)src_mesh->primitives_count; ++i) {
      owl_i32 j;
      owl_i32 vertices_count = 0;
      owl_i32 indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv = NULL;
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
        vertices_count = (int)attribute->data->count;
      }

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "NORMAL")))
        normal = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "TEXCOORD_0")))
        uv = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "JOINTS_0")))
        joints0 = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(src_primitive, "WEIGHTS_0")))
        weights0 = owl_resolve_gltf_accessor_(attribute->data);

      for (j = 0; j < vertices_count; ++j) {
        owl_i32 offset = sli->vertices_count;
        struct owl_model_vertex *vertex = &sli->vertices[offset + j];

        OWL_V3_COPY(&position[j * 3], vertex->position);

        if (normal)
          OWL_V3_COPY(&normal[j * 3], vertex->normal);
        else
          OWL_V3_ZERO(vertex->normal);

        if (uv)
          OWL_V2_COPY(&uv[j * 2], vertex->uv);
        else
          OWL_V2_ZERO(vertex->uv);

        OWL_V3_SET(1.0F, 1.0F, 1.0F, vertex->color);

        if (joints0 && weights0)
          OWL_V4_COPY(&joints0[j * 4], vertex->joints0);
        else
          OWL_V4_ZERO(vertex->joints0);

        if (joints0 && weights0)
          OWL_V4_COPY(&weights0[j * 4], vertex->weights0);
        else
          OWL_V4_ZERO(vertex->weights0);
      }

      indices_count = (int)src_primitive->indices->count;

      switch (src_primitive->indices->component_type) {
      case cgltf_component_type_r_32u: {
        owl_i32 offset = sli->indices_count;
        owl_u32 const *indices =
            owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (int)src_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[j] + (owl_u32)sli->vertices_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        owl_i32 offset = sli->indices_count;
        owl_u16 const *indices =
            owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (int)src_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[j] + (owl_u16)sli->vertices_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        owl_i32 offset = sli->indices_count;
        owl_u8 const *indices =
            owl_resolve_gltf_accessor_(src_primitive->indices);
        for (j = 0; j < (int)src_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[j] + (owl_u8)sli->vertices_count;
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_UNKNOWN;
        goto end;
      }

      dst_primitive->first = (owl_u32)sli->indices_count;
      dst_primitive->count = (owl_u32)indices_count;
      dst_primitive->material.slot =
          (int)(src_primitive->material - gltf->materials);

      sli->indices_count += indices_count;
      sli->vertices_count += vertices_count;
    }
  }

end:
  return code;
}

OWL_INTERNAL void owl_model_load_buffers_(struct owl_renderer *r,
                                          struct owl_model_load_desc const *sli,
                                          struct owl_model *model) {
  struct owl_renderer_dynamic_heap_reference vrdhr;
  struct owl_renderer_dynamic_heap_reference irdhr;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(r));
  OWL_ASSERT(sli->vertices_count == sli->vertices_capacity);
  OWL_ASSERT(sli->indices_count == sli->indices_capacity);

  owl_renderer_invalidate_dynamic_heap(r);

  {
    owl_u64 size;

    size = (owl_u64)sli->vertices_capacity * sizeof(struct owl_model_vertex);
    owl_renderer_dynamic_heap_submit(r, size, sli->vertices, &vrdhr);

    size = (owl_u64)sli->indices_capacity * sizeof(owl_u32);
    owl_renderer_dynamic_heap_submit(r, size, sli->indices, &irdhr);
  }

  {
    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size =
        (owl_u64)sli->vertices_count * sizeof(struct owl_model_vertex);
    buffer.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &model->vertices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetBufferMemoryRequirements(r->device, model->vertices_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &model->vertices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->vertices_buffer,
                                    model->vertices_memory, 0));
  }

  {
    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size = (owl_u64)sli->indices_count * sizeof(owl_u32);
    buffer.usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &model->indices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetBufferMemoryRequirements(r->device, model->indices_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &model->indices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->indices_buffer,
                                    model->indices_memory, 0));
  }

  if (OWL_SUCCESS != (code = owl_renderer_flush_dynamic_heap(r)))
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_begin_immidiate_command_buffer(r)))
    goto end;

  {
    VkBufferCopy copy;

    copy.srcOffset = vrdhr.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)sli->vertices_count * sizeof(struct owl_model_vertex);

    vkCmdCopyBuffer(r->immidiate_command_buffer, vrdhr.buffer,
                    model->vertices_buffer, 1, &copy);
  }

  {
    VkBufferCopy copy;

    copy.srcOffset = irdhr.offset;
    copy.dstOffset = 0;
    copy.size = (owl_u64)sli->indices_count * sizeof(owl_u32);

    vkCmdCopyBuffer(r->immidiate_command_buffer, irdhr.buffer,
                    model->indices_buffer, 1, &copy);
  }

  if (OWL_SUCCESS != (code = owl_renderer_end_immidiate_command_buffer(r)))
    goto end;

  owl_renderer_clear_dynamic_heap_offset(r);

end:
  OWL_ASSERT(OWL_SUCCESS == code);
  return;
}

OWL_INTERNAL enum owl_code owl_model_load_nodes_(struct owl_renderer *r,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  owl_i32 i;
  struct owl_model_load_desc sli;
  struct cgltf_scene const *src_scene;
  enum owl_code code = OWL_SUCCESS;

  src_scene = gltf->scene;

  if (OWL_SUCCESS != (code = owl_model_init_load_desc_(r, gltf, &sli)))
    goto end;

  for (i = 0; i < OWL_MODEL_MAX_NODES_COUNT; ++i) {
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].parent.slot = -1;
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].skin.slot = -1;
  }

  if (OWL_MODEL_MAX_NODES_COUNT <= (int)gltf->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_desc;
  }

  model->nodes_count = (int)gltf->nodes_count;

  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    code = owl_model_load_node_(r, gltf, &gltf->nodes[i], &sli, model);

    if (OWL_SUCCESS != code)
      goto end_err_deinit_load_desc;
  }

  if (OWL_MODEL_MAX_NODE_ROOTS_COUNT <= (int)src_scene->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_desc;
  }

  model->roots_count = (int)src_scene->nodes_count;

  for (i = 0; i < (int)src_scene->nodes_count; ++i)
    model->roots[i].slot = (int)(src_scene->nodes[i] - gltf->nodes);

  owl_model_load_buffers_(r, &sli, model);

end_err_deinit_load_desc:
  owl_model_deinit_load_desc_(r, &sli);

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_model_load_skins_(struct owl_renderer *r,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_SKINS_COUNT <= (int)gltf->skins_count) {
    OWL_ASSERT(0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->skins_count = (int)gltf->skins_count;

  for (i = 0; i < (int)gltf->skins_count; ++i) {
    owl_i32 j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *src_skin = &gltf->skins[i];
    struct owl_model_skin_data *dst_skin = &model->skins[i];

    if (src_skin->name)
      OWL_STRNCPY(dst_skin->name, src_skin->name, OWL_MODEL_MAX_NAME_LENGTH);
    else
      OWL_STRNCPY(dst_skin->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);

    dst_skin->skeleton_root.slot = (int)(src_skin->skeleton - gltf->nodes);

    if (OWL_MODEL_SKIN_MAX_JOINTS_COUNT <= (int)src_skin->joints_count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_skin->joints_count = (int)src_skin->joints_count;

    for (j = 0; j < (int)src_skin->joints_count; ++j) {

      dst_skin->joints[j].slot = (int)(src_skin->joints[j] - gltf->nodes);

      OWL_ASSERT(!OWL_STRNCMP(model->nodes[dst_skin->joints[j].slot].name,
                              src_skin->joints[j]->name,
                              OWL_MODEL_MAX_NAME_LENGTH));
    }

    dst_skin->inverse_bind_matrices_count = 0;

    if (!src_skin->inverse_bind_matrices)
      continue;

    if (OWL_MODEL_SKIN_MAX_INVERSE_BIND_MATRICES_COUNT <=
        (int)src_skin->inverse_bind_matrices->count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_skin->inverse_bind_matrices_count =
        (int)src_skin->inverse_bind_matrices->count;

    inverse_bind_matrices =
        owl_resolve_gltf_accessor_(src_skin->inverse_bind_matrices);

    for (j = 0; j < (int)src_skin->inverse_bind_matrices->count; ++j)
      OWL_M4_COPY(inverse_bind_matrices[j], dst_skin->inverse_bind_matrices[j]);

    {
      VkBufferCreateInfo buffer;

      dst_skin->ssbo_buffer_size =
          src_skin->inverse_bind_matrices->count * sizeof(owl_m4);

      buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer.pNext = NULL;
      buffer.flags = 0;
      buffer.size = dst_skin->ssbo_buffer_size;
      buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      buffer.queueFamilyIndexCount = 0;
      buffer.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkCreateBuffer(r->device, &buffer, NULL,
                                    &dst_skin->ssbo_buffers[j]));
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo memory;

      vkGetBufferMemoryRequirements(r->device, dst_skin->ssbo_buffers[0],
                                    &requirements);

      dst_skin->ssbo_buffer_alignment = requirements.alignment;
      dst_skin->ssbo_buffer_aligned_size =
          OWL_ALIGNU2(dst_skin->ssbo_buffer_size, requirements.alignment);

      memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memory.pNext = NULL;
      memory.allocationSize = dst_skin->ssbo_buffer_aligned_size *
                              OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      memory.memoryTypeIndex = owl_renderer_find_memory_type(
          r, &requirements, OWL_MEMORY_VISIBILITY_CPU_COHERENT);

      OWL_VK_CHECK(
          vkAllocateMemory(r->device, &memory, NULL, &dst_skin->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkBindBufferMemory(
            r->device, dst_skin->ssbo_buffers[j], dst_skin->ssbo_memory,
            (owl_u64)j * dst_skin->ssbo_buffer_aligned_size));
    }

    {
      VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
      VkDescriptorSetAllocateInfo sets;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        layouts[j] = r->joints_set_layout;

      sets.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      sets.pNext = NULL;
      sets.descriptorPool = r->common_set_pool;
      sets.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      sets.pSetLayouts = layouts;

      OWL_VK_CHECK(
          vkAllocateDescriptorSets(r->device, &sets, dst_skin->ssbo_sets));
    }

    {

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        VkDescriptorBufferInfo buffer;
        VkWriteDescriptorSet write;

        buffer.buffer = dst_skin->ssbo_buffers[j];
        buffer.offset = 0;
        buffer.range = dst_skin->ssbo_buffer_size;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = NULL;
        write.dstSet = dst_skin->ssbo_sets[j];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pImageInfo = NULL;
        write.pBufferInfo = &buffer;
        write.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
      }
    }

    {
      owl_i32 k;
      void *data;
      owl_byte *bytes;

      vkMapMemory(r->device, dst_skin->ssbo_memory, 0, VK_WHOLE_SIZE, 0, &data);

      bytes = data;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        dst_skin->ssbo_datas[j] =
            (owl_m4 *)&bytes[(owl_u64)j * dst_skin->ssbo_buffer_aligned_size];

      for (k = 0; k < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++k)
        for (j = 0; j < (int)src_skin->inverse_bind_matrices->count; ++j)
          OWL_M4_COPY(inverse_bind_matrices[j], dst_skin->ssbo_datas[k][j]);
    }
  }

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_animations_(struct owl_renderer *r,
                           struct cgltf_data const *gltf,
                           struct owl_model *model) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_ANIMATIONS_COUNT <= (int)gltf->animations_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->animations_count = (int)gltf->animations_count;

  for (i = 0; i < (int)gltf->animations_count; ++i) {
    owl_i32 j;
    struct cgltf_animation const *src_animation;
    struct owl_model_animation_data *dst_animation;

    src_animation = &gltf->animations[i];
    dst_animation = &model->animations[i];

    dst_animation->current_time = 0.0F;

    if (OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT <=
        (int)src_animation->samplers_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_animation->samplers_count = (int)src_animation->samplers_count;

    dst_animation->begin = FLT_MAX;
    dst_animation->end = FLT_MIN;

    for (j = 0; j < (int)src_animation->samplers_count; ++j) {
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
          (int)src_sampler->input->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_sampler->inputs_count = (int)src_sampler->input->count;

      inputs = owl_resolve_gltf_accessor_(src_sampler->input);

      for (k = 0; k < (int)src_sampler->input->count; ++k) {
        float const input = inputs[k];

        dst_sampler->inputs[k] = input;

        if (input < dst_animation->begin)
          dst_animation->begin = input;

        if (input > dst_animation->end)
          dst_animation->end = input;
      }

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT <=
          (int)src_sampler->output->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      dst_sampler->outputs_count = (int)src_sampler->output->count;

      switch (src_sampler->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor_(src_sampler->output);

        for (k = 0; k < (int)src_sampler->output->count; ++k) {
          OWL_V4_ZERO(dst_sampler->outputs[k]);
          OWL_V3_COPY(outputs[k], dst_sampler->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor_(src_sampler->output);
        for (k = 0; k < (int)src_sampler->output->count; ++k)
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

      dst_sampler->interpolation = (int)src_sampler->interpolation;

      dst_animation->samplers[j].slot = sampler.slot;
    }

    if (OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT <=
        (int)src_animation->channels_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    dst_animation->channels_count = (int)src_animation->channels_count;

    for (j = 0; j < (int)src_animation->channels_count; ++j) {
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

      dst_channel->path = (int)src_channel->target_path;
      dst_channel->node.slot = (int)(src_channel->target_node - gltf->nodes);
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

enum owl_code owl_model_init(struct owl_renderer *r, char const *path,
                             struct owl_model *model) {
  struct cgltf_options options;
  struct cgltf_data *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(r));

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

  if (OWL_SUCCESS != (code = owl_model_load_images_(r, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_textures_(r, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_materials_(r, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_nodes_(r, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_skins_(r, data, model)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_model_load_animations_(r, data, model)))
    goto end_err_free_data;

end_err_free_data:
  cgltf_free(data);

  owl_renderer_clear_dynamic_heap_offset(r);

end:
  return code;
}

void owl_model_deinit(struct owl_renderer *r, struct owl_model *model) {
  owl_i32 i;

  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeMemory(r->device, model->indices_memory, NULL);
  vkDestroyBuffer(r->device, model->indices_buffer, NULL);
  vkFreeMemory(r->device, model->vertices_memory, NULL);
  vkDestroyBuffer(r->device, model->vertices_buffer, NULL);

  for (i = 0; i < model->images_count; ++i)
    owl_renderer_deinit_image(r, &model->images[i].image);

  for (i = 0; i < model->skins_count; ++i) {
    owl_i32 j;

    vkFreeDescriptorSets(r->device, r->common_set_pool,
                         OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                         model->skins[i].ssbo_sets);

    vkFreeMemory(r->device, model->skins[i].ssbo_memory, NULL);

    for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
      vkDestroyBuffer(r->device, model->skins[i].ssbo_buffers[j], NULL);
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
owl_model_node_update_joints_(struct owl_renderer const *r,
                              struct owl_model *model,
                              struct owl_model_node const *node) {
  owl_i32 i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin_data const *dst_skin;
  struct owl_model_node_data const *dst_node;

  dst_node = &model->nodes[node->slot];

  for (i = 0; i < dst_node->children_count; ++i)
    owl_model_node_update_joints_(r, model, &dst_node->children[i]);

  if (OWL_MODEL_NODE_NO_SKIN_SLOT == dst_node->skin.slot)
    goto end;

  dst_skin = &model->skins[dst_node->skin.slot];

  owl_model_resolve_node_matrix_(model, node, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < dst_skin->joints_count; ++i) {
    owl_model_resolve_node_matrix_(model, &dst_skin->joints[i], tmp);
    owl_m4_multiply(tmp, dst_skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp,
                    dst_skin->ssbo_datas[r->active_frame_index][i]);
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

void owl_model_update_animation(struct owl_renderer const *r,
                                struct owl_model *model, float dt) {
  owl_i32 i;
  struct owl_model_animation_data *dst_animation;

  if (OWL_MODEL_NO_ANIMATION_SLOT == model->active_animation.slot)
    goto end;

  dst_animation = &model->animations[model->active_animation.slot];

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
        dst_sampler->interpolation)
      continue;

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
      }
    }
  }

  for (i = 0; i < model->roots_count; ++i) {
    owl_model_node_update_joints_(r, model, &model->roots[i]);
  }

end:
  return;
}

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
  int i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (int)gltf->images_count; ++i) {
    char uri[OWL_MAX_URI_SIZE];
    struct owl_image_init_info iii;
    struct owl_model_image_data *image = &model->images[i];

    if (OWL_SUCCESS != (code = owl_fix_uri_(gltf->images[i].uri, uri)))
      goto end;

    iii.source_type = OWL_IMAGE_SOURCE_TYPE_FILE;
    iii.path = uri;
    iii.use_default_sampler = 1;

    if (OWL_SUCCESS != (code = owl_image_init(r, &iii, &image->image)))
      goto end;
  }

  model->images_count = (int)gltf->images_count;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_textures_(struct owl_renderer *r, struct cgltf_data const *gltf,
                         struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  model->textures_count = (int)gltf->textures_count;

  for (i = 0; i < (int)gltf->textures_count; ++i) {
    struct cgltf_texture const *from_texture = &gltf->textures[i];
    model->textures[i].image.slot = (int)(from_texture->image - gltf->images);
  }

  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_materials_(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_MATERIALS_COUNT <= (int)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->materials_count = (int)gltf->materials_count;

  for (i = 0; i < (int)gltf->materials_count; ++i) {
    struct cgltf_material const *from_material = &gltf->materials[i];
    struct owl_model_material_data *material_data = &model->materials[i];

    OWL_ASSERT(from_material->has_pbr_metallic_roughness);

    OWL_V4_COPY(from_material->pbr_metallic_roughness.base_color_factor,
                material_data->base_color_factor);

    material_data->base_color_texture.slot =
        (int)(from_material->pbr_metallic_roughness.base_color_texture.texture -
              gltf->textures);

#if 0
    OWL_ASSERT(from_material->normal_texture.texture);

    material_data->normal_texture.slot =
        (int)(from_material->normal_texture.texture - gltf->textures);
#else
    /* HACK(samuel): currently we don't check for the normal texture */
    material_data->normal_texture.slot = material_data->base_color_texture.slot;
#endif

    /* FIXME(samuel): make sure this is right */
    material_data->alpha_mode = (enum owl_alpha_mode)from_material->alpha_mode;
    material_data->alpha_cutoff = from_material->alpha_cutoff;
    material_data->double_sided = from_material->double_sided;
  }

end:
  return code;
}

struct owl_model_load_info {
  int vertices_capacity;
  int vertices_count;
  struct owl_model_vertex *vertices;

  int indices_capacity;
  int indices_count;
  owl_u32 *indices;
};

OWL_INTERNAL struct cgltf_attribute const *
owl_find_gltf_attribute_(struct cgltf_primitive const *from_primitive,
                         char const *attribute_name) {
  int i;
  struct cgltf_attribute const *attribute = NULL;

  for (i = 0; i < (int)from_primitive->attributes_count; ++i) {
    struct cgltf_attribute const *current = &from_primitive->attributes[i];

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
owl_model_find_load_info_capacities_(struct cgltf_data const *gltf,
                                     struct owl_model_load_info *sli) {
  int i;
  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    int j;
    struct cgltf_node const *from_node = &gltf->nodes[i];

    if (!from_node->mesh)
      continue;

    for (j = 0; j < (int)from_node->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attribute;
      struct cgltf_primitive const *primitive = &from_node->mesh->primitives[j];

      attribute = owl_find_gltf_attribute_(primitive, "POSITION");
      sli->vertices_capacity += attribute->data->count;
      sli->indices_capacity += primitive->indices->count;
    }
  }
}

OWL_INTERNAL enum owl_code
owl_model_init_load_info_(struct owl_renderer *r, struct cgltf_data const *gltf,
                          struct owl_model_load_info *sli) {
  VkDeviceSize size;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  sli->vertices_capacity = 0;
  sli->indices_capacity = 0;
  sli->vertices_count = 0;
  sli->indices_count = 0;

  owl_model_find_load_info_capacities_(gltf, sli);

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

OWL_INTERNAL void owl_model_deinit_load_info_(struct owl_renderer *r,
                                              struct owl_model_load_info *sli) {
  OWL_UNUSED(r);

  OWL_FREE(sli->indices);
  OWL_FREE(sli->vertices);
}

OWL_INTERNAL enum owl_code
owl_model_load_node_(struct owl_renderer *r, struct cgltf_data const *gltf,
                     struct cgltf_node const *from_node,
                     struct owl_model_load_info *sli, struct owl_model *model) {

  int i;
  struct owl_model_node node;
  struct owl_model_node_data *node_data;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  node.slot = (int)(from_node - gltf->nodes);

  if (OWL_MODEL_MAX_NODES_COUNT <= node.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  node_data = &model->nodes[node.slot];

  if (from_node->parent)
    node_data->parent.slot = (int)(from_node->parent - gltf->nodes);
  else
    node_data->parent.slot = OWL_MODEL_NODE_NO_PARENT_SLOT;

  if (OWL_MODEL_NODE_MAX_CHILDREN_COUNT <= (int)from_node->children_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  node_data->children_count = (int)from_node->children_count;

  for (i = 0; i < (int)from_node->children_count; ++i)
    node_data->children[i].slot = (int)(from_node->children[i] - gltf->nodes);

  if (from_node->name)
    OWL_STRNCPY(node_data->name, from_node->name, OWL_MODEL_MAX_NAME_LENGTH);
  else
    OWL_STRNCPY(node_data->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);

  if (from_node->has_translation)
    OWL_V3_COPY(from_node->translation, node_data->translation);
  else
    OWL_V3_ZERO(node_data->translation);

  if (from_node->has_rotation)
    OWL_V4_COPY(from_node->rotation, node_data->rotation);
  else
    OWL_V4_ZERO(node_data->rotation);

  if (from_node->has_scale)
    OWL_V3_COPY(from_node->scale, node_data->scale);
  else
    OWL_V3_SET(1.0F, 1.0F, 1.0F, node_data->scale);

  if (from_node->has_matrix)
    OWL_M4_COPY_V16(from_node->matrix, node_data->matrix);
  else
    OWL_M4_IDENTITY(node_data->matrix);

  if (from_node->skin)
    node_data->skin.slot = (int)(from_node->skin - gltf->skins);
  else
    node_data->skin.slot = OWL_MODEL_NODE_NO_SKIN_SLOT;

  if (from_node->mesh) {
    struct cgltf_mesh const *from_mesh;
    struct owl_model_mesh_data *mesh_data;

    node_data->mesh.slot = model->meshes_count++;

    if (OWL_MODEL_MAX_MESHES_COUNT <= node_data->mesh.slot) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    from_mesh = from_node->mesh;
    mesh_data = &model->meshes[node_data->mesh.slot];

    mesh_data->primitives_count = (int)from_mesh->primitives_count;

    for (i = 0; i < (int)from_mesh->primitives_count; ++i) {
      int j;
      int vertices_count = 0;
      int indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv = NULL;
      owl_u16 const *joints0 = NULL;
      float const *weights0 = NULL;
      struct cgltf_attribute const *attribute = NULL;
      struct owl_model_primitive *primitive = NULL;
      struct owl_model_primitive_data *primitive_data = NULL;
      struct cgltf_primitive const *from_primitive = &from_mesh->primitives[i];

      primitive = &mesh_data->primitives[i];
      primitive->slot = model->primitives_count++;

      if (OWL_MODEL_MAX_PRIMITIVES_COUNT <= primitive->slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      primitive_data = &model->primitives[primitive->slot];

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "POSITION"))) {
        position = owl_resolve_gltf_accessor_(attribute->data);
        vertices_count = (int)attribute->data->count;
      }

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "NORMAL")))
        normal = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "TEXCOORD_0")))
        uv = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "JOINTS_0")))
        joints0 = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "WEIGHTS_0")))
        weights0 = owl_resolve_gltf_accessor_(attribute->data);

      for (j = 0; j < vertices_count; ++j) {
        int offset = sli->vertices_count;
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

      indices_count = (int)from_primitive->indices->count;

      switch (from_primitive->indices->component_type) {
      case cgltf_component_type_r_32u: {
        int offset = sli->indices_count;
        owl_u32 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[j] + (owl_u32)sli->vertices_count;
        }
      } break;

      case cgltf_component_type_r_16u: {
        int offset = sli->indices_count;
        owl_u16 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[j] + (owl_u16)sli->vertices_count;
        }
      } break;

      case cgltf_component_type_r_8u: {
        int offset = sli->indices_count;
        owl_u8 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
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

      primitive_data->first = (owl_u32)sli->indices_count;
      primitive_data->count = (owl_u32)indices_count;
      primitive_data->material.slot =
          (int)(from_primitive->material - gltf->materials);

      sli->indices_count += indices_count;
      sli->vertices_count += vertices_count;
    }
  }

end:
  return code;
}

OWL_INTERNAL void owl_model_load_buffers_(struct owl_renderer *r,
                                          struct owl_model_load_info const *sli,
                                          struct owl_model *model) {
  struct owl_dynamic_heap_reference vdhr;
  struct owl_dynamic_heap_reference idhr;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(r));
  OWL_ASSERT(sli->vertices_count == sli->vertices_capacity);
  OWL_ASSERT(sli->indices_count == sli->indices_capacity);

  {
    owl_byte *data;
    VkDeviceSize size;

    size = (owl_u64)sli->vertices_capacity * sizeof(struct owl_model_vertex);
    data = owl_renderer_dynamic_heap_alloc(r, size, &vdhr);
    OWL_MEMCPY(data, sli->vertices, size);

    size = (owl_u64)sli->indices_capacity * sizeof(owl_u32);
    data = owl_renderer_dynamic_heap_alloc(r, size, &idhr);
    OWL_MEMCPY(data, sli->indices, size);
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
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

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
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &model->indices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, model->indices_buffer,
                                    model->indices_memory, 0));
  }

  {
    struct owl_single_use_command_buffer sucb;
    owl_renderer_init_single_use_command_buffer(r, &sucb);

    {
      VkBufferCopy copy;

      copy.srcOffset = vdhr.offset;
      copy.dstOffset = 0;
      copy.size =
          (owl_u64)sli->vertices_count * sizeof(struct owl_model_vertex);

      vkCmdCopyBuffer(sucb.command_buffer, vdhr.buffer, model->vertices_buffer,
                      1, &copy);
    }

    {
      VkBufferCopy copy;

      copy.srcOffset = idhr.offset;
      copy.dstOffset = 0;
      copy.size = (owl_u64)sli->indices_count * sizeof(owl_u32);

      vkCmdCopyBuffer(sucb.command_buffer, idhr.buffer, model->indices_buffer,
                      1, &copy);
    }

    owl_renderer_deinit_single_use_command_buffer(r, &sucb);
  }

  owl_renderer_clear_dynamic_heap_offset(r);
}

OWL_INTERNAL enum owl_code owl_model_load_nodes_(struct owl_renderer *r,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  int i;
  struct owl_model_load_info sli;
  struct cgltf_scene const *from_scene;
  enum owl_code code = OWL_SUCCESS;

  from_scene = gltf->scene;

  if (OWL_SUCCESS != (code = owl_model_init_load_info_(r, gltf, &sli)))
    goto end;

  for (i = 0; i < OWL_MODEL_MAX_NODES_COUNT; ++i) {
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].parent.slot = -1;
    model->nodes[i].mesh.slot = -1;
    model->nodes[i].skin.slot = -1;
  }

  if (OWL_MODEL_MAX_NODES_COUNT <= (int)gltf->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_info;
  }

  model->nodes_count = (int)gltf->nodes_count;

  for (i = 0; i < (int)gltf->nodes_count; ++i) {
    code = owl_model_load_node_(r, gltf, &gltf->nodes[i], &sli, model);

    if (OWL_SUCCESS != code)
      goto end_err_deinit_load_info;
  }

  if (OWL_MODEL_MAX_NODE_ROOTS_COUNT <= (int)from_scene->nodes_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end_err_deinit_load_info;
  }

  model->roots_count = (int)from_scene->nodes_count;

  for (i = 0; i < (int)from_scene->nodes_count; ++i)
    model->roots[i].slot = (int)(from_scene->nodes[i] - gltf->nodes);

  owl_model_load_buffers_(r, &sli, model);

end_err_deinit_load_info:
  owl_model_deinit_load_info_(r, &sli);

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_model_load_skins_(struct owl_renderer *r,
                                                 struct cgltf_data const *gltf,
                                                 struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_SKINS_COUNT <= (int)gltf->skins_count) {
    OWL_ASSERT(0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->skins_count = (int)gltf->skins_count;

  for (i = 0; i < (int)gltf->skins_count; ++i) {
    int j;
    owl_m4 const *inverse_bind_matrices;
    struct cgltf_skin const *from_skin = &gltf->skins[i];
    struct owl_model_skin_data *skin_data = &model->skins[i];

    if (from_skin->name)
      OWL_STRNCPY(skin_data->name, from_skin->name, OWL_MODEL_MAX_NAME_LENGTH);
    else
      OWL_STRNCPY(skin_data->name, "NO NAME", OWL_MODEL_MAX_NAME_LENGTH);

    skin_data->skeleton_root.slot = (int)(from_skin->skeleton - gltf->nodes);

    if (OWL_MODEL_SKIN_MAX_JOINTS_COUNT <= (int)from_skin->joints_count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    skin_data->joints_count = (int)from_skin->joints_count;

    for (j = 0; j < (int)from_skin->joints_count; ++j) {

      skin_data->joints[j].slot = (int)(from_skin->joints[j] - gltf->nodes);

      OWL_ASSERT(!OWL_STRNCMP(model->nodes[skin_data->joints[j].slot].name,
                              from_skin->joints[j]->name,
                              OWL_MODEL_MAX_NAME_LENGTH));
    }

    skin_data->inverse_bind_matrices_count = 0;

    if (!from_skin->inverse_bind_matrices)
      continue;

    if (OWL_MODEL_SKIN_MAX_INVERSE_BIND_MATRICES_COUNT <=
        (int)from_skin->inverse_bind_matrices->count) {
      OWL_ASSERT(0);
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    skin_data->inverse_bind_matrices_count =
        (int)from_skin->inverse_bind_matrices->count;

    inverse_bind_matrices =
        owl_resolve_gltf_accessor_(from_skin->inverse_bind_matrices);

    for (j = 0; j < (int)from_skin->inverse_bind_matrices->count; ++j)
      OWL_M4_COPY(inverse_bind_matrices[j],
                  skin_data->inverse_bind_matrices[j]);

    {
      VkBufferCreateInfo buffer;

      skin_data->ssbo_buffer_size =
          from_skin->inverse_bind_matrices->count * sizeof(owl_m4);

      buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      buffer.pNext = NULL;
      buffer.flags = 0;
      buffer.size = skin_data->ssbo_buffer_size;
      buffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
      buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      buffer.queueFamilyIndexCount = 0;
      buffer.pQueueFamilyIndices = NULL;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkCreateBuffer(r->device, &buffer, NULL,
                                    &skin_data->ssbo_buffers[j]));
    }

    {
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo memory;

      vkGetBufferMemoryRequirements(r->device, skin_data->ssbo_buffers[0],
                                    &requirements);

      skin_data->ssbo_buffer_alignment = requirements.alignment;
      skin_data->ssbo_buffer_aligned_size =
          OWL_ALIGNU2(skin_data->ssbo_buffer_size, requirements.alignment);

      memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      memory.pNext = NULL;
      memory.allocationSize = skin_data->ssbo_buffer_aligned_size *
                              OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
      memory.memoryTypeIndex = owl_renderer_find_memory_type(
          r, &requirements, OWL_MEMORY_VISIBILITY_CPU_ONLY);

      OWL_VK_CHECK(
          vkAllocateMemory(r->device, &memory, NULL, &skin_data->ssbo_memory));

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        OWL_VK_CHECK(vkBindBufferMemory(
            r->device, skin_data->ssbo_buffers[j], skin_data->ssbo_memory,
            (owl_u64)j * skin_data->ssbo_buffer_aligned_size));
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
          vkAllocateDescriptorSets(r->device, &sets, skin_data->ssbo_sets));
    }

    {

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j) {
        VkDescriptorBufferInfo buffer;
        VkWriteDescriptorSet write;

        buffer.buffer = skin_data->ssbo_buffers[j];
        buffer.offset = 0;
        buffer.range = skin_data->ssbo_buffer_size;

        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = NULL;
        write.dstSet = skin_data->ssbo_sets[j];
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
      int k;
      void *data;
      owl_byte *bytes;

      vkMapMemory(r->device, skin_data->ssbo_memory, 0, VK_WHOLE_SIZE, 0,
                  &data);

      bytes = data;

      for (j = 0; j < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++j)
        skin_data->ssbo_datas[j] =
            (owl_m4 *)&bytes[(owl_u64)j * skin_data->ssbo_buffer_aligned_size];

      for (k = 0; k < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++k)
        for (j = 0; j < (int)from_skin->inverse_bind_matrices->count; ++j)
          OWL_M4_COPY(inverse_bind_matrices[j], skin_data->ssbo_datas[k][j]);
    }
  }

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_model_load_animations_(struct owl_renderer *r,
                           struct cgltf_data const *gltf,
                           struct owl_model *model) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_MODEL_MAX_ANIMATIONS_COUNT <= (int)gltf->animations_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  model->animations_count = (int)gltf->animations_count;

  for (i = 0; i < (int)gltf->animations_count; ++i) {
    int j;
    struct cgltf_animation const *from_animation;
    struct owl_model_animation_data *animation_data;

    from_animation = &gltf->animations[i];
    animation_data = &model->animations[i];

    animation_data->current_time = 0.0F;

    if (OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT <=
        (int)from_animation->samplers_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    animation_data->samplers_count = (int)from_animation->samplers_count;

    animation_data->begin = FLT_MAX;
    animation_data->end = FLT_MIN;

    for (j = 0; j < (int)from_animation->samplers_count; ++j) {
      int k;
      float const *inputs;
      struct cgltf_animation_sampler const *from_sampler;
      struct owl_model_animation_sampler sampler;
      struct owl_model_animation_sampler_data *sampler_data;

      from_sampler = &from_animation->samplers[j];

      sampler.slot = model->animation_samplers_count++;

      if (OWL_MODEL_MAX_SAMPLERS_COUNT <= sampler.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      sampler_data = &model->animation_samplers[sampler.slot];

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_INPUTS_COUNT <=
          (int)from_sampler->input->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      sampler_data->inputs_count = (int)from_sampler->input->count;

      inputs = owl_resolve_gltf_accessor_(from_sampler->input);

      for (k = 0; k < (int)from_sampler->input->count; ++k) {
        float const input = inputs[k];

        sampler_data->inputs[k] = input;

        if (input < animation_data->begin)
          animation_data->begin = input;

        if (input > animation_data->end)
          animation_data->end = input;
      }

      if (OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT <=
          (int)from_sampler->output->count) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      sampler_data->outputs_count = (int)from_sampler->output->count;

      switch (from_sampler->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor_(from_sampler->output);

        for (k = 0; k < (int)from_sampler->output->count; ++k) {
          OWL_V4_ZERO(sampler_data->outputs[k]);
          OWL_V3_COPY(outputs[k], sampler_data->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor_(from_sampler->output);
        for (k = 0; k < (int)from_sampler->output->count; ++k)
          OWL_V4_COPY(outputs[k], sampler_data->outputs[k]);
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

      sampler_data->interpolation = (int)from_sampler->interpolation;

      animation_data->samplers[j].slot = sampler.slot;
    }

    if (OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT <=
        (int)from_animation->channels_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    animation_data->channels_count = (int)from_animation->channels_count;

    for (j = 0; j < (int)from_animation->channels_count; ++j) {
      struct cgltf_animation_channel const *from_channel;
      struct owl_model_animation_channel channel;
      struct owl_model_animation_channel_data *channel_data;

      from_channel = &from_animation->channels[j];

      channel.slot = model->animation_channels_count++;

      if (OWL_MODEL_MAX_CHANNELS_COUNT <= channel.slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      channel_data = &model->animation_channels[channel.slot];

      channel_data->path = (int)from_channel->target_path;
      channel_data->node.slot = (int)(from_channel->target_node - gltf->nodes);
      channel_data->animation_sampler.slot =
          animation_data
              ->samplers[(from_channel->sampler - from_animation->samplers)]
              .slot;

      animation_data->channels[j].slot = channel.slot;
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
  int i;

  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeMemory(r->device, model->indices_memory, NULL);
  vkDestroyBuffer(r->device, model->indices_buffer, NULL);
  vkFreeMemory(r->device, model->vertices_memory, NULL);
  vkDestroyBuffer(r->device, model->vertices_buffer, NULL);

  for (i = 0; i < model->images_count; ++i)
    owl_image_deinit(r, &model->images[i].image);

  for (i = 0; i < model->skins_count; ++i) {
    int j;

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
  struct owl_model_node_data const *node_data;

  node_data = &model->nodes[node->slot];

  OWL_M4_IDENTITY(matrix);
  owl_m4_translate(node_data->translation, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_q4_as_m4(node_data->rotation, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_m4_scale(tmp, node_data->scale, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  owl_m4_multiply(matrix, node_data->matrix, matrix);
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
  int i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_skin_data const *skin_data;
  struct owl_model_node_data const *node_data;

  node_data = &model->nodes[node->slot];

  for (i = 0; i < node_data->children_count; ++i)
    owl_model_node_update_joints_(r, model, &node_data->children[i]);

  if (OWL_MODEL_NODE_NO_SKIN_SLOT == node_data->skin.slot)
    goto end;

  skin_data = &model->skins[node_data->skin.slot];

  owl_model_resolve_node_matrix_(model, node, tmp);
  owl_m4_inverse(tmp, inverse);

  for (i = 0; i < skin_data->joints_count; ++i) {
    owl_model_resolve_node_matrix_(model, &skin_data->joints[i], tmp);
    owl_m4_multiply(tmp, skin_data->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp, skin_data->ssbo_datas[r->frame][i]);
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
  int i;
  struct owl_model_animation_data *animation_data;

  if (OWL_MODEL_NO_ANIMATION_SLOT == model->active_animation.slot)
    goto end;

  animation_data = &model->animations[model->active_animation.slot];

  if (animation_data->end < (animation_data->current_time += dt))
    animation_data->current_time -= animation_data->end;

  for (i = 0; i < animation_data->channels_count; ++i) {
    int j;
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
        sampler_data->interpolation)
      continue;

    for (j = 0; j < sampler_data->inputs_count - 1; ++j) {
      float const i0 = sampler_data->inputs[j];
      float const i1 = sampler_data->inputs[j + 1];
      float const ct = animation_data->current_time;
      float const a = (ct - i0) / (i1 - i0);

      if (!((ct >= i0) && (ct <= i1)))
        continue;

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
      }
    }
  }

  for (i = 0; i < model->roots_count; ++i) {
    owl_model_node_update_joints_(r, model, &model->roots[i]);
  }

end:
  return;
}

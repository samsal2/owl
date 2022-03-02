
#include "scene.h"

#include "internal.h"
#include "owl/types.h"
#include "renderer.h"
#include "texture.h"
#include "vector_math.h"

#include <cgltf/cgltf.h>
#include <stdio.h>

#define OWL_MAX_URI_SIZE 128

OWL_INTERNAL enum owl_code owl_fix_uri_(char const *uri,
                                        char fixed[OWL_MAX_URI_SIZE]) {
  enum owl_code code = OWL_SUCCESS;

  snprintf(fixed, OWL_MAX_URI_SIZE, "../../assets/%s", uri);

  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_images_(struct owl_renderer *r,
                                                  cgltf_data const *data,
                                                  struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (int)data->images_count; ++i) {
    char uri[128];
    struct owl_texture_init_info tii;
    struct owl_texture *t = &scene->images[i];

    if (OWL_SUCCESS != (code = owl_fix_uri_(data->images[i].uri, uri)))
      goto end;

    if (OWL_SUCCESS != (code = owl_texture_init_from_file(r, &tii, uri, t)))
      goto end;
  }

  scene->images_count = (int)data->images_count;

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_textures_(struct owl_renderer *r,
                                                    cgltf_data const *data,
                                                    struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  for (i = 0; i < (int)data->textures_count; ++i) {
    cgltf_texture const *from = &data->textures[i];
    scene->textures[i] = (int)(from->image - data->images);
  }

  scene->textures_count = (int)data->textures_count;

  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_materials_(struct owl_renderer *r,
                                                     cgltf_data const *data,
                                                     struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_SCENE_MAX_MATERIALS_COUNT <= (int)data->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  for (i = 0; i < (int)data->materials_count; ++i) {
    cgltf_material const *from = &data->materials[i];
    struct owl_scene_material *material = &scene->materials[i];

    OWL_ASSERT(from->has_pbr_metallic_roughness);

    OWL_V4_COPY(from->pbr_metallic_roughness.base_color_factor,
                material->base_color_factor);

    material->base_color_texture =
        (int)(from->pbr_metallic_roughness.base_color_texture.texture -
              data->textures);

    material->normal_texture =
        (int)(from->normal_texture.texture - data->textures);

    /* FIXME(samuel): make sure this is right */
    material->alpha_mode = (enum owl_alpha_mode)from->alpha_mode;
    material->alpha_cutoff = from->alpha_cutoff;
    material->double_sided = from->double_sided;
  }

  scene->materials_count = (int)data->materials_count;

end:
  return code;
}

struct owl_scene_load_info {
  struct owl_dynamic_heap_reference vertices_reference;
  int vertices_capacity;
  int vertices_count;
  struct owl_scene_vertex *vertices;

  struct owl_dynamic_heap_reference indices_reference;
  int indices_capacity;
  int indices_count;
  owl_u32 *indices;
};

OWL_INTERNAL cgltf_attribute const *
owl_find_cgltf_attribute_(cgltf_primitive const *primitive,
                          char const *attribute_name) {
  int i;
  cgltf_attribute const *attribute = NULL;

  for (i = 0; i < (int)primitive->attributes_count; ++i) {
    cgltf_attribute const *tmp = &primitive->attributes[i];

    if (!strcmp(tmp->name, attribute_name)) {
      attribute = tmp;
      goto end;
    }
  }

end:
  return attribute;
}

OWL_INTERNAL void
owl_scene_find_load_info_capacities_(cgltf_node const *node,
                                     struct owl_scene_load_info *sli) {
  int i;
  cgltf_mesh const *mesh = NULL;
  cgltf_attribute const *attribute = NULL;
  cgltf_accessor const *accessor = NULL;

  for (i = 0; i < (int)node->children_count; ++i)
    owl_scene_find_load_info_capacities_(node->children[i], sli);

  if (!node->mesh)
    return;

  mesh = node->mesh;

  for (i = 0; i < (int)mesh->primitives_count; ++i) {
    cgltf_primitive const *primitive = &mesh->primitives[i];
    attribute = owl_find_cgltf_attribute_(primitive, "POSITION");
    accessor = attribute->data;
    sli->vertices_capacity += accessor->count;

    accessor = primitive->indices;
    sli->indices_capacity += accessor->count;
  }
}

OWL_INTERNAL void owl_scene_setup_load_info_(struct owl_renderer *r,
                                             cgltf_data const *data,
                                             struct owl_scene_load_info *sli) {
  int i;
  VkDeviceSize size;

  sli->vertices_capacity = 0;
  sli->indices_capacity = 0;
  sli->vertices_count = 0;
  sli->indices_count = 0;

  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_scene_find_load_info_capacities_(&data->nodes[i], sli);

  size = (owl_u64)sli->vertices_capacity * sizeof(struct owl_scene_vertex);
  sli->vertices = owl_renderer_dynamic_heap_alloc(r, size, &sli->vertices_reference);

  size = (owl_u64)sli->indices_capacity * sizeof(owl_u32);
  sli->indices = owl_renderer_dynamic_heap_alloc(r, size, &sli->indices_reference);
}

OWL_INTERNAL enum owl_code
owl_scene_load_node_(struct owl_renderer *r, cgltf_data const *cgltf,
                     cgltf_node const *from_node, owl_scene_node parent,
                     struct owl_scene_load_info *sli, struct owl_scene *scene) {

  int i;
  enum owl_code code = OWL_SUCCESS;
  owl_scene_node node_index = scene->nodes_count++;
  struct owl_scene_node_data *node = &scene->nodes[node_index];

  node->parent = parent;

  if (OWL_SCENE_NODE_NO_PARENT == parent) {
    scene->roots[scene->roots_count++] = node_index;
  } else {
    struct owl_scene_node_data *parent_data = &scene->nodes[parent];
    parent_data->children[parent_data->children_count++] = node_index;
  }

  if (from_node->has_matrix)
    OWL_M4_COPY_V16(from_node->matrix, node->matrix);
  else
    OWL_M4_IDENTITY(node->matrix);

  for (i = 0; i < (int)from_node->children_count; ++i) {
    code = owl_scene_load_node_(r, cgltf, from_node->children[i], node_index,
                                sli, scene);

    if (OWL_SUCCESS != code)
      goto end;
  }

  if (OWL_SCENE_MAX_MESHES_COUNT <= (node->mesh = scene->meshes_count++)) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  scene->meshes[node->mesh].primitives_count = 0;

  if (from_node->mesh) {
    cgltf_mesh const *from_mesh = from_node->mesh;
    struct owl_scene_mesh_data *mesh = &scene->meshes[node->mesh];

    for (i = 0; i < (int)from_mesh->primitives_count; ++i) {
      int j;
      int vertices_count = 0;
      int indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv = NULL;
      float const *tangent = NULL;
      owl_byte const *b = NULL;
      cgltf_buffer_view const *view = NULL;
      cgltf_accessor const *accessor = NULL;
      cgltf_attribute const *attribute = NULL;
      struct owl_scene_primitive *primitive = &mesh->primitives[i];
      cgltf_primitive const *from_primitive = &from_mesh->primitives[i];

      attribute = owl_find_cgltf_attribute_(from_primitive, "POSITION");

      if (attribute) {
        accessor = attribute->data;
        view = accessor->buffer_view;
        b = (owl_byte *)view->buffer->data;
        position = (float const *)(&b[accessor->offset + view->offset]);
        vertices_count = (int)accessor->count;
      }

      attribute = owl_find_cgltf_attribute_(from_primitive, "NORMAL");

      if (attribute) {
        accessor = attribute->data;
        view = accessor->buffer_view;
        b = (owl_byte *)view->buffer->data;
        normal = (float const *)(&b[accessor->offset + view->offset]);
      }

      attribute = owl_find_cgltf_attribute_(from_primitive, "TEXCOORD_0");

      if (attribute) {
        accessor = attribute->data;
        view = accessor->buffer_view;
        b = (owl_byte *)view->buffer->data;
        uv = (float const *)(&b[accessor->offset + view->offset]);
      }

      attribute = owl_find_cgltf_attribute_(from_primitive, "TANGENT");

      if (attribute) {
        accessor = attribute->data;
        view = accessor->buffer_view;
        b = (owl_byte *)view->buffer->data;
        tangent = (float const *)(&b[accessor->offset + view->offset]);
      }

      for (j = 0; j < vertices_count; ++j) {
        int offset = sli->vertices_count;
        struct owl_scene_vertex *vertex = &sli->vertices[offset + j];

        OWL_V3_COPY(&position[j * 3], vertex->position);
        /* HACK(flipping the model Y coordinate */
        vertex->position[1] *= -1.0F;

        if (normal)
          OWL_V3_COPY(&normal[j * 3], vertex->normal);
        else
          OWL_V3_ZERO(vertex->normal);

        if (uv)
          OWL_V2_COPY(&uv[j * 2], vertex->uv);
        else
          OWL_V2_ZERO(vertex->uv);

        OWL_V3_SET(0.0F, 1.0F, 1.0F, vertex->color);

        if (tangent)
          OWL_V4_COPY(&tangent[j * 4], vertex->tangent);
        else
          OWL_V4_ZERO(vertex->tangent);
      }

      accessor = from_primitive->indices;
      view = accessor->buffer_view;

      indices_count = (int)accessor->count;

      switch (accessor->component_type) {
      case cgltf_component_type_r_32u: {
        int offset = sli->indices_count;
        owl_u32 const *indices =
            (owl_u32 const *)(&b[view->offset + accessor->offset]);
        for (j = 0; j < (int)accessor->count; ++j) {
          sli->indices[offset + j] = indices[sli->vertices_count + j];
        }
      } break;

      case cgltf_component_type_r_16u: {
        int offset = sli->indices_count;
        owl_u16 const *indices =
            (owl_u16 const *)(&b[view->offset + accessor->offset]);
        for (j = 0; j < (int)accessor->count; ++j) {
          sli->indices[offset + j] = indices[sli->vertices_count + j];
        }
      } break;

      case cgltf_component_type_r_8u: {
        int offset = sli->indices_count;
        owl_u8 const *indices =
            (owl_u8 const *)(&b[view->offset + accessor->offset]);
        for (j = 0; j < (int)accessor->count; ++j) {
          sli->indices[offset + j] = indices[sli->vertices_count + j];
        }
      } break;

      case cgltf_component_type_invalid:
      case cgltf_component_type_r_8:
      case cgltf_component_type_r_16:
      case cgltf_component_type_r_32f:
        code = OWL_ERROR_UNKNOWN;
        goto end;
      }

      primitive->first = (owl_u32)sli->indices_count;
      primitive->count = (owl_u32)indices_count;
      primitive->material = (int)(from_primitive->material - cgltf->materials);

      sli->indices_count += accessor->count;
      sli->vertices_count += vertices_count;
    }

    mesh->primitives_count = (int)from_mesh->primitives_count;
  }

end:
  return code;
}

OWL_INTERNAL void owl_scene_load_buffers_(struct owl_renderer *r,
                                          struct owl_scene_load_info const *sli,
                                          struct owl_scene *scene) {
  OWL_ASSERT(sli->vertices_count == sli->vertices_capacity);
  OWL_ASSERT(sli->indices_count == sli->indices_capacity);

  {
    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size =
        (owl_u64)sli->vertices_count * sizeof(struct owl_scene_vertex);
    buffer.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = 0;

    OWL_VK_CHECK(
        vkCreateBuffer(r->device, &buffer, NULL, &scene->vertices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetBufferMemoryRequirements(r->device, scene->vertices_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &scene->vertices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, scene->vertices_buffer,
                                    scene->vertices_memory, 0));
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
        vkCreateBuffer(r->device, &buffer, NULL, &scene->indices_buffer));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetBufferMemoryRequirements(r->device, scene->indices_buffer,
                                  &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &scene->indices_memory));
    OWL_VK_CHECK(vkBindBufferMemory(r->device, scene->indices_buffer,
                                    scene->indices_memory, 0));
  }

  {
    struct owl_single_use_command_buffer sucb;
    owl_renderer_init_single_use_command_buffer(r, &sucb);

    {
      VkBufferCopy copy;

      copy.srcOffset = sli->vertices_reference.offset;
      copy.dstOffset = 0;
      copy.size =
          (owl_u64)sli->vertices_count * sizeof(struct owl_scene_vertex);

      vkCmdCopyBuffer(sucb.command_buffer, sli->vertices_reference.buffer,
                      scene->vertices_buffer, 1, &copy);
    }

    {
      VkBufferCopy copy;

      copy.srcOffset = sli->indices_reference.offset;
      copy.dstOffset = 0;
      copy.size = (owl_u64)sli->indices_count * sizeof(owl_u32);

      vkCmdCopyBuffer(sucb.command_buffer, sli->indices_reference.buffer,
                      scene->indices_buffer, 1, &copy);
    }

    owl_renderer_deinit_single_use_command_buffer(r, &sucb);
  }

  owl_renderer_clear_dynamic_heap_offset(r);
}

enum owl_code owl_scene_init(struct owl_renderer *r, char const *path,
                             struct owl_scene *scene) {

  int i;
  cgltf_options options;
  cgltf_data *data = NULL;
  struct owl_scene_load_info sli;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(r));

  scene->roots_count = 0;
  scene->nodes_count = 0;
  scene->images_count = 0;
  scene->textures_count = 0;
  scene->materials_count = 0;
  scene->meshes_count = 0;

  OWL_MEMSET(&options, 0, sizeof(options));

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data)) {
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    code = OWL_ERROR_UNKNOWN;
    goto end_err_free_data;
  }

  if (OWL_SUCCESS != (code = owl_scene_load_images_(r, data, scene)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_scene_load_textures_(r, data, scene)))
    goto end_err_free_data;

  if (OWL_SUCCESS != (code = owl_scene_load_materials_(r, data, scene)))
    goto end_err_free_data;

  owl_scene_setup_load_info_(r, data, &sli);

  for (i = 0; i < (int)data->nodes_count; ++i)
    owl_scene_load_node_(r, data, &data->nodes[i], OWL_SCENE_NODE_NO_PARENT,
                         &sli, scene);

  owl_scene_load_buffers_(r, &sli, scene);

end_err_free_data:
  cgltf_free(data);

  owl_renderer_clear_dynamic_heap_offset(r);

end:
  return code;
}

void owl_scene_deinit(struct owl_renderer *r, struct owl_scene *scene) {
  int i;

  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeMemory(r->device, scene->indices_memory, NULL);
  vkDestroyBuffer(r->device, scene->indices_buffer, NULL);
  vkFreeMemory(r->device, scene->vertices_memory, NULL);
  vkDestroyBuffer(r->device, scene->vertices_buffer, NULL);

  for (i = 0; i < scene->images_count; ++i)
    owl_texture_deinit(r, &scene->images[i]);
}

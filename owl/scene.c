#include "scene.h"

#include "internal.h"
#include "renderer.h"
#include "types.h"
#include "vector_math.h"

#include <cgltf/cgltf.h>
#include <float.h>
#include <stdio.h>

#define OWL_MAX_URI_SIZE 128

OWL_INTERNAL void const *
owl_resolve_gltf_accessor_(cgltf_accessor const *accessor) {
  cgltf_buffer_view const *view = accessor->buffer_view;
  owl_byte const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

OWL_INTERNAL enum owl_code owl_fix_uri_(char const *uri,
                                        char fixed[OWL_MAX_URI_SIZE]) {
  enum owl_code code = OWL_SUCCESS;

  snprintf(fixed, OWL_MAX_URI_SIZE, "../../assets/%s", uri);

  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_images_(struct owl_renderer *r,
                                                  cgltf_data const *gltf,
                                                  struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (int)gltf->images_count; ++i) {
    char uri[128];
    struct owl_image_init_info iii;
    struct owl_scene_image_data *image = &scene->images[i];

    if (OWL_SUCCESS != (code = owl_fix_uri_(gltf->images[i].uri, uri)))
      goto end;

    iii.source_type = OWL_IMAGE_SOURCE_TYPE_FILE;
    iii.path = uri;
    iii.use_default_sampler = 1;

    if (OWL_SUCCESS != (code = owl_image_init(r, &iii, &image->image)))
      goto end;
  }

  scene->images_count = (int)gltf->images_count;

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_textures_(struct owl_renderer *r,
                                                    cgltf_data const *gltf,
                                                    struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  scene->textures_count = (int)gltf->textures_count;

  for (i = 0; i < (int)gltf->textures_count; ++i) {
    cgltf_texture const *from_texture = &gltf->textures[i];
    scene->textures[i].image.slot = (int)(from_texture->image - gltf->images);
  }

  return code;
}

OWL_INTERNAL enum owl_code owl_scene_load_materials_(struct owl_renderer *r,
                                                     cgltf_data const *gltf,
                                                     struct owl_scene *scene) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  if (OWL_SCENE_MAX_MATERIALS_COUNT <= (int)gltf->materials_count) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  scene->materials_count = (int)gltf->materials_count;

  for (i = 0; i < (int)gltf->materials_count; ++i) {
    cgltf_material const *from_material = &gltf->materials[i];
    struct owl_scene_material_data *material_data = &scene->materials[i];

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

struct owl_scene_load_info {
  int vertices_capacity;
  int vertices_count;
  struct owl_scene_vertex *vertices;
  struct owl_dynamic_heap_reference vertices_reference;

  int indices_capacity;
  int indices_count;
  owl_u32 *indices;
  struct owl_dynamic_heap_reference indices_reference;
};

OWL_INTERNAL cgltf_attribute const *
owl_find_gltf_attribute_(cgltf_primitive const *from_primitive,
                         char const *attribute_name) {
  int i;
  cgltf_attribute const *attribute = NULL;

  for (i = 0; i < (int)from_primitive->attributes_count; ++i) {
    cgltf_attribute const *tmp = &from_primitive->attributes[i];

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
    attribute = owl_find_gltf_attribute_(primitive, "POSITION");
    accessor = attribute->data;
    sli->vertices_capacity += accessor->count;

    accessor = primitive->indices;
    sli->indices_capacity += accessor->count;
  }
}

OWL_INTERNAL void owl_scene_setup_load_info_(struct owl_renderer *r,
                                             cgltf_data const *gltf,
                                             struct owl_scene_load_info *sli) {
  int i;
  VkDeviceSize size;

  sli->vertices_capacity = 0;
  sli->indices_capacity = 0;
  sli->vertices_count = 0;
  sli->indices_count = 0;

  for (i = 0; i < (int)gltf->nodes_count; ++i)
    owl_scene_find_load_info_capacities_(&gltf->nodes[i], sli);

  size = (owl_u64)sli->vertices_capacity * sizeof(struct owl_scene_vertex);
  sli->vertices =
      owl_renderer_dynamic_heap_alloc(r, size, &sli->vertices_reference);

  size = (owl_u64)sli->indices_capacity * sizeof(owl_u32);
  sli->indices =
      owl_renderer_dynamic_heap_alloc(r, size, &sli->indices_reference);
}

OWL_INTERNAL enum owl_code
owl_scene_load_node_(struct owl_renderer *r, cgltf_data const *gltf,
                     cgltf_node const *from_node,
                     struct owl_scene_node const *parent,
                     struct owl_scene_load_info *sli, struct owl_scene *scene) {

  int i;
  struct owl_scene_node node;
  struct owl_scene_node_data *node_data;
  enum owl_code code = OWL_SUCCESS;

  node.slot = scene->nodes_count++;

  if (OWL_SCENE_MAX_NODES_COUNT <= node.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  node_data = &scene->nodes[node.slot];

  node_data->parent.slot = parent->slot;

  if (OWL_SCENE_NODE_NO_PARENT_SLOT == parent->slot) {
    struct owl_scene_node root;

    root.slot = scene->roots_count++;

    if (OWL_SCENE_MAX_NODE_ROOTS_COUNT <= root.slot) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    scene->roots[root.slot].slot = node.slot;
  } else {
    struct owl_scene_node child;
    struct owl_scene_node_data *parent_data = &scene->nodes[parent->slot];

    child.slot = parent_data->children_count++;

    if (OWL_SCENE_NODE_MAX_CHILDREN_COUNT <= child.slot) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    parent_data->children[child.slot].slot = node.slot;
  }

  if (from_node->has_matrix)
    OWL_M4_COPY_V16(from_node->matrix, node_data->matrix);
  else
    OWL_M4_IDENTITY(node_data->matrix);

  for (i = 0; i < (int)from_node->children_count; ++i) {
    code = owl_scene_load_node_(r, gltf, from_node->children[i], &node, sli,
                                scene);

    if (OWL_SUCCESS != code)
      goto end;
  }

  node_data->mesh.slot = scene->meshes_count++;

  if (OWL_SCENE_MAX_MESHES_COUNT <= node_data->mesh.slot) {
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto end;
  }

  scene->meshes[node_data->mesh.slot].primitives_count = 0;

  if (from_node->mesh) {
    cgltf_mesh const *from_mesh = from_node->mesh;
    struct owl_scene_mesh_data *mesh_data =
        &scene->meshes[node_data->mesh.slot];

    mesh_data->primitives_count = (int)from_mesh->primitives_count;

    for (i = 0; i < (int)from_mesh->primitives_count; ++i) {
      int j;
      int vertices_count = 0;
      int indices_count = 0;
      float const *position = NULL;
      float const *normal = NULL;
      float const *uv = NULL;
      float const *tangent = NULL;
      cgltf_attribute const *attribute = NULL;
      struct owl_scene_primitive *primitive = NULL;
      struct owl_scene_primitive_data *primitive_data = NULL;
      cgltf_primitive const *from_primitive = &from_mesh->primitives[i];

      primitive = &mesh_data->primitives[i];
      primitive->slot = scene->primitives_count++;

      if (OWL_SCENE_MAX_PRIMITIVES_COUNT <= primitive->slot) {
        code = OWL_ERROR_OUT_OF_BOUNDS;
        goto end;
      }

      primitive_data = &scene->primitives[primitive->slot];

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "POSITION"))) {
        position = owl_resolve_gltf_accessor_(attribute->data);
        vertices_count = (int)attribute->data->count;
      }

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "NORMAL")))
        normal = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "TEXCOORD_0")))
        uv = owl_resolve_gltf_accessor_(attribute->data);

      if ((attribute = owl_find_gltf_attribute_(from_primitive, "TANGENT")))
        tangent = owl_resolve_gltf_accessor_(attribute->data);

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

        OWL_V3_SET(1.0F, 1.0F, 1.0F, vertex->color);

        if (tangent)
          OWL_V4_COPY(&tangent[j * 4], vertex->tangent);
        else
          OWL_V4_ZERO(vertex->tangent);
      }

      indices_count = (int)from_primitive->indices->count;

      switch (from_primitive->indices->component_type) {
      case cgltf_component_type_r_32u: {
        int offset = sli->indices_count;
        owl_u32 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[sli->vertices_count + j];
        }
      } break;

      case cgltf_component_type_r_16u: {
        int offset = sli->indices_count;
        owl_u16 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
          sli->indices[offset + j] = indices[sli->vertices_count + j];
        }
      } break;

      case cgltf_component_type_r_8u: {
        int offset = sli->indices_count;
        owl_u8 const *indices =
            owl_resolve_gltf_accessor_(from_primitive->indices);
        for (j = 0; j < (int)from_primitive->indices->count; ++j) {
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
  struct owl_scene_node root;
  struct owl_scene_load_info sli;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(r));

  scene->roots_count = 0;
  scene->nodes_count = 0;
  scene->images_count = 0;
  scene->textures_count = 0;
  scene->materials_count = 0;
  scene->meshes_count = 0;
  scene->primitives_count = 0;
  scene->skins_count = 0;
  scene->animation_samplers_count = 0;
  scene->animation_channels_count = 0;
  scene->animations_count = 0;

  root.slot = OWL_SCENE_NODE_NO_PARENT_SLOT;

  OWL_MEMSET(scene, 0, sizeof(*scene));
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

  for (i = 0; i < (int)data->nodes_count; ++i) {
    code = owl_scene_load_node_(r, data, &data->nodes[i], &root, &sli, scene);

    if (OWL_SUCCESS != code)
      goto end_err_free_data;
  }

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
    owl_image_deinit(r, &scene->images[i].image);
}

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
  char path[256];
};

static void const *
owl_resolve_gltf_accessor(struct cgltf_accessor const *accessor) {
  struct cgltf_buffer_view const *view = accessor->buffer_view;
  uint8_t const *data = view->buffer->data;
  return &data[accessor->offset + view->offset];
}

static int owl_model_get_real_uri(struct owl_model *m, char const *src,
                                  struct owl_model_uri *uri) {
  int ret = OWL_OK;

  snprintf(uri->path, sizeof(uri->path), "%s/%s", m->directory, src);

  return ret;
}

static int owl_model_load_images(struct owl_renderer *r,
                                 struct cgltf_data *gltf,
                                 struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;

  OWL_ASSERT(gltf->images_count < OWL_ARRAY_SIZE(m->images));

  m->num_images = (int32_t)gltf->images_count;

  OWL_DEBUG_LOG("loading images\n");

  for (i = 0; i < m->num_images; ++i) {
    struct owl_model_uri uri;
    struct owl_texture_desc desc;
    struct owl_model_image *out_image = &m->images[i];
    struct cgltf_image *in_image = &gltf->images[i];

    ret = owl_model_get_real_uri(m, in_image->uri, &uri);
    OWL_ASSERT(!ret);

    OWL_DEBUG_LOG("  trying %s\n", uri.path);

    desc.source = OWL_TEXTURE_SOURCE_FILE;
    desc.type = OWL_TEXTURE_TYPE_2D;
    desc.path = uri.path;
    desc.width = 0;
    desc.height = 0;
    desc.format = OWL_RGBA8_SRGB;

    ret = owl_texture_init(r, &desc, &out_image->texture);
    OWL_ASSERT(!ret);
  }

  return ret;
}

static void owl_model_unload_images(struct owl_renderer *r,
                                    struct owl_model *m) {
  int32_t i;

  for (i = 0; i < m->num_images; ++i)
    owl_texture_deinit(r, &m->images[i].texture);
}

static int owl_model_load_textures(struct owl_renderer *r,
                                   struct cgltf_data const *gltf,
                                   struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;

  OWL_UNUSED(r);

  OWL_ASSERT(gltf->textures_count < OWL_ARRAY_SIZE(m->textures));

  m->num_textures = (int32_t)gltf->textures_count;

  for (i = 0; i < m->num_textures; ++i) {
    struct cgltf_texture const *texture = &gltf->textures[i];
    m->textures[i].image = (int32_t)(texture->image - gltf->images);
  }

  return ret;
}

static void owl_model_unload_textures(struct owl_renderer *r,
                                      struct owl_model *m) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
}

static int owl_model_load_materials(struct owl_renderer *r,
                                    struct cgltf_data *gltf,
                                    struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;

  OWL_UNUSED(r);

  OWL_ASSERT(gltf->materials_count < OWL_ARRAY_SIZE(m->materials));

  /* TODO(samuel): fix custom static assert */
  OWL_ASSERT(OWL_ALPHA_MODE_OPAQUE == cgltf_alpha_mode_opaque &&
             "must match with cgltf enums");
  OWL_ASSERT(OWL_ALPHA_MODE_MASK == cgltf_alpha_mode_mask &&
             "must match with cgltf enums");
  OWL_ASSERT(OWL_ALPHA_MODE_BLEND == cgltf_alpha_mode_blend &&
             "must match with cgltf enums");

  m->num_materials = (int32_t)gltf->materials_count;

  for (i = 0; i < m->num_materials; ++i) {
    struct owl_model_material *out_material = &m->materials[i];
    struct cgltf_material *in_material = &gltf->materials[i];

    out_material->double_sided = in_material->double_sided;
    out_material->alpha_cutoff = 1.0F;
    out_material->emissive_factor[0] = 1.0F;
    out_material->emissive_factor[1] = 1.0F;
    out_material->emissive_factor[2] = 1.0F;
    out_material->emissive_factor[3] = 1.0F;
    out_material->base_color_texcoord = 0;
    out_material->metallic_roughness_texcoord = 0;
    out_material->specular_glossiness_texcoord = 0;
    out_material->normal_texcoord = 0;
    out_material->emissive_texcoord = 0;
    out_material->base_color_texture = -1;
    out_material->base_color_texcoord = -1;

    out_material->metallic_roughness_texture = -1;
    out_material->metallic_roughness_texcoord = -1;

    out_material->roughness_factor = 1.0F;
    out_material->metallic_factor = 1.0F;

    out_material->base_color_factor[0] = 1.0F;
    out_material->base_color_factor[1] = 1.0F;
    out_material->base_color_factor[2] = 1.0F;
    out_material->base_color_factor[3] = 1.0F;

    out_material->metallic_roughness_enable =
        in_material->has_pbr_metallic_roughness;
    out_material->specular_glossiness_enable =
        in_material->has_pbr_specular_glossiness;

    if (in_material->has_pbr_metallic_roughness) {
      /* i really dislike how convoluted this is */
      struct cgltf_texture *texture;
      struct cgltf_texture *first;
      struct cgltf_texture_view *view;
      struct cgltf_pbr_metallic_roughness *metallic;
      metallic = &in_material->pbr_metallic_roughness;

      first = gltf->textures;

      view = &metallic->base_color_texture;
      texture = view->texture;
      if (texture) {
        out_material->base_color_texture = (int32_t)(texture - first);
        out_material->base_color_texcoord = view->texcoord;
      } else {
        out_material->base_color_texture = -1;
        out_material->base_color_texcoord = 0;
      }

      view = &metallic->metallic_roughness_texture;
      texture = view->texture;
      if (texture) {
        out_material->metallic_roughness_texture = (int32_t)(texture - first);
        out_material->metallic_roughness_texcoord = view->texcoord;
      } else {
        out_material->metallic_roughness_texture = -1;
        out_material->metallic_roughness_texcoord = 0;
      }

      out_material->roughness_factor = metallic->roughness_factor;
      out_material->metallic_factor = metallic->metallic_factor;

      out_material->base_color_factor[0] = metallic->base_color_factor[0];
      out_material->base_color_factor[1] = metallic->base_color_factor[1];
      out_material->base_color_factor[2] = metallic->base_color_factor[2];
      out_material->base_color_factor[3] = metallic->base_color_factor[3];
    }

    {
      struct cgltf_texture *texture;
      struct cgltf_texture *first;
      struct cgltf_texture_view *view;

      first = gltf->textures;

      view = &in_material->normal_texture;
      texture = view->texture;
      if (texture) {
        out_material->normal_texture = (int32_t)(texture - first);
        out_material->normal_texcoord = view->texcoord;
      } else {
        out_material->normal_texture = -1;
        out_material->normal_texcoord = 0;
      }

      view = &in_material->emissive_texture;
      texture = view->texture;
      if (texture) {
        out_material->emissive_texture = (int32_t)(texture - first);
        out_material->emissive_texcoord = view->texcoord;
      } else {
        out_material->emissive_texture = -1;
        out_material->emissive_texcoord = 0;
      }

      view = &in_material->occlusion_texture;
      texture = view->texture;
      if (texture) {
        out_material->occlusion_texture = (int32_t)(texture - first);
        out_material->occlusion_texcoord = view->texcoord;
      } else {
        out_material->occlusion_texture = -1;
        out_material->occlusion_texcoord = 0;
      }

      out_material->alpha_mode = in_material->alpha_mode;
      out_material->alpha_cutoff = in_material->alpha_cutoff;

      out_material->emissive_factor[0] = in_material->emissive_factor[0];
      out_material->emissive_factor[1] = in_material->emissive_factor[1];
      out_material->emissive_factor[2] = in_material->emissive_factor[2];
      out_material->emissive_factor[3] = 1.0F;
    }

    if (in_material->has_pbr_specular_glossiness) {
      struct cgltf_texture *texture;
      struct cgltf_texture *first;
      struct cgltf_texture_view *view;
      struct cgltf_pbr_specular_glossiness *specular;
      specular = &in_material->pbr_specular_glossiness;

      first = gltf->textures;

      view = &specular->specular_glossiness_texture;
      texture = view->texture;
      if (texture) {
        out_material->specular_glossiness_texture = (int32_t)(texture - first);
        out_material->specular_glossiness_texcoord = view->texcoord;
      } else {
        out_material->specular_glossiness_texture = -1;
        out_material->specular_glossiness_texcoord = 0;
      }

      view = &specular->diffuse_texture;
      texture = view->texture;
      if (texture)
        out_material->diffuse_texture = (int32_t)(texture - first);
      else
        out_material->diffuse_texture = -1;

      out_material->diffuse_factor[0] = specular->diffuse_factor[0];
      out_material->diffuse_factor[1] = specular->diffuse_factor[1];
      out_material->diffuse_factor[2] = specular->diffuse_factor[2];
      out_material->diffuse_factor[3] = specular->diffuse_factor[3];

      out_material->specular_factor[0] = specular->specular_factor[0];
      out_material->specular_factor[1] = specular->specular_factor[1];
      out_material->specular_factor[2] = specular->specular_factor[2];
      out_material->specular_factor[3] = 1.0F;
    }

    {
      VkDescriptorSetAllocateInfo info;
      VkResult vk_result;
      OWL_UNUSED(vk_result);

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = r->descriptor_pool;
      info.descriptorSetCount = 1;
      info.pSetLayouts = &r->model_maps_descriptor_set_layout;

      vk_result = vkAllocateDescriptorSets(r->device, &info,
                                           &out_material->descriptor_set);
      OWL_ASSERT(!vk_result);
    }

    {
      uint32_t j;
      VkDescriptorImageInfo descriptors[6];
      VkWriteDescriptorSet writes[6];

      OWL_ASSERT(m->empty_texture.image_view);

      descriptors[0].sampler = r->linear_sampler;
      descriptors[0].imageView = VK_NULL_HANDLE;
      descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[1].sampler = VK_NULL_HANDLE;
      descriptors[1].imageView = m->empty_texture.image_view;
      descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[2].sampler = VK_NULL_HANDLE;
      descriptors[2].imageView = m->empty_texture.image_view;
      descriptors[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      if (out_material->specular_glossiness_enable) {
        if (-1 != out_material->diffuse_texture) {
          struct owl_model_texture *texture;
          struct owl_model_image *image;
          texture = &m->textures[out_material->diffuse_texture];
          image = &m->images[m->textures[texture->image].image];
          descriptors[1].imageView = image->texture.image_view;
        }

        if (-1 != out_material->specular_glossiness_texture) {
          struct owl_model_texture *texture;
          struct owl_model_image *image;
          texture = &m->textures[out_material->specular_glossiness_texture];
          image = &m->images[m->textures[texture->image].image];
          descriptors[2].imageView = image->texture.image_view;
        }

      } else if (out_material->metallic_roughness_enable) {
        if (-1 != out_material->base_color_texture) {
          struct owl_model_texture *texture;
          struct owl_model_image *image;
          texture = &m->textures[out_material->base_color_texture];
          image = &m->images[texture->image];
          descriptors[1].imageView = image->texture.image_view;
        }

        if (-1 != out_material->metallic_roughness_texture) {
          struct owl_model_texture *texture;
          struct owl_model_image *image;
          texture = &m->textures[out_material->metallic_roughness_texture];
          image = &m->images[texture->image];
          descriptors[2].imageView = image->texture.image_view;
        }
      }

      descriptors[3].sampler = VK_NULL_HANDLE;
      if (-1 != out_material->normal_texture) {
        struct owl_model_texture *texture;
        struct owl_model_image *image;
        texture = &m->textures[out_material->normal_texture];
        image = &m->images[texture->image];
        descriptors[3].imageView = image->texture.image_view;
      } else {
        descriptors[3].imageView = m->empty_texture.image_view;
      }
      descriptors[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[4].sampler = VK_NULL_HANDLE;
      if (-1 != out_material->occlusion_texture) {
        struct owl_model_texture *texture;
        struct owl_model_image *image;
        texture = &m->textures[out_material->occlusion_texture];
        image = &m->images[texture->image];
        descriptors[4].imageView = image->texture.image_view;
      } else {
        descriptors[4].imageView = m->empty_texture.image_view;
      }
      descriptors[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      descriptors[5].sampler = VK_NULL_HANDLE;
      if (-1 != out_material->emissive_texture) {
        struct owl_model_texture *texture;
        struct owl_model_image *image;
        texture = &m->textures[out_material->emissive_texture];
        image = &m->images[texture->image];
        descriptors[5].imageView = image->texture.image_view;
      } else {
        descriptors[5].imageView = m->empty_texture.image_view;
      }
      descriptors[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      for (j = 0; j < OWL_ARRAY_SIZE(writes); ++j) {
        writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[j].pNext = NULL;
        writes[j].dstSet = out_material->descriptor_set;
        writes[j].dstBinding = j;
        writes[j].dstArrayElement = 0;
        writes[j].descriptorCount = 1;
        if (0 == j)
          writes[j].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        else
          writes[j].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[j].pImageInfo = &descriptors[j];
        writes[j].pBufferInfo = NULL;
        writes[j].pTexelBufferView = NULL;
      }

      vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0,
                             NULL);
    }
  }

  return ret;
}

static void owl_model_unload_materials(struct owl_renderer *r,
                                       struct owl_model *m) {
  int32_t i;
  VkDevice const device = r->device;
  for (i = 0; i < m->num_materials; ++i) {
    struct owl_model_material *material = &m->materials[i];
    vkFreeDescriptorSets(device, r->descriptor_pool, 1,
                         &material->descriptor_set);
  }
}

static struct cgltf_attribute const *
owl_find_gltf_attribute(struct cgltf_primitive const *p, char const *name) {
  uint32_t i;

  for (i = 0; i < p->attributes_count; ++i) {
    struct cgltf_attribute const *current = &p->attributes[i];

    if (!OWL_STRNCMP(current->name, name, 256))
      return current;
  }

  return NULL;
}

struct owl_model_all_primitives {
  int32_t num_vertices;
  struct owl_model_vertex *vertices;

  int32_t num_indices;
  uint32_t *indices;
};

static int owl_model_init_all_primitives(struct owl_model_all_primitives *p,
                                         struct cgltf_data const *gltf) {
  uint32_t i;
  int ret = OWL_OK;

  p->num_vertices = 0;
  p->num_indices = 0;

  for (i = 0; i < gltf->nodes_count; ++i) {
    uint32_t j;
    struct cgltf_node const *node = &gltf->nodes[i];

    if (!node->mesh)
      continue;

    for (j = 0; j < node->mesh->primitives_count; ++j) {
      struct cgltf_attribute const *attr;
      struct cgltf_primitive const *primitive;

      primitive = &node->mesh->primitives[j];
      attr = owl_find_gltf_attribute(primitive, "POSITION");

      p->num_vertices += attr->data->count;
      p->num_indices += primitive->indices->count;
    }
  }

  p->vertices = OWL_MALLOC(p->num_vertices * sizeof(*p->vertices));
  OWL_ASSERT(p->vertices);

  if (p->num_indices) {
    p->indices = OWL_MALLOC(p->num_indices * sizeof(*p->indices));
    OWL_ASSERT(p->indices);
  } else {
    p->indices = NULL;
  }

  return ret;
}

static void
owl_model_deinit_all_primitives(struct owl_model_all_primitives *p) {
  OWL_FREE(p->indices);
  OWL_FREE(p->vertices);
}

static int32_t owl_model_gltf_stride(cgltf_type type) {
  switch (type) {

  case cgltf_type_invalid:
    return 0;
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
  }
}

static int owl_model_load_nodes(struct owl_renderer *r,
                                struct cgltf_data const *gltf,
                                struct owl_model_all_primitives *p,
                                struct owl_model *m) {
  int32_t i;
  int32_t num_vertices = 0;
  int32_t num_indices = 0;
  int ret = OWL_OK;
  VkDevice const device = r->device;

  OWL_UNUSED(r);

  OWL_ASSERT(gltf->nodes_count < OWL_ARRAY_SIZE(m->nodes));
  OWL_ASSERT(gltf->meshes_count < OWL_ARRAY_SIZE(m->meshes));

  m->num_primitives = 0;
  m->num_meshes = 0;
  m->num_nodes = (int32_t)gltf->nodes_count;

  for (i = 0; i < m->num_nodes; ++i) {
    int32_t j;
    struct cgltf_node const *in_node;
    struct owl_model_node *out_node;

    in_node = &gltf->nodes[i];
    out_node = &m->nodes[(int32_t)(in_node - gltf->nodes)];

    if (in_node->parent)
      out_node->parent = (int32_t)(in_node->parent - gltf->nodes);
    else
      out_node->parent = -1;

    OWL_ASSERT(in_node->children_count < OWL_ARRAY_SIZE(out_node->children));

    out_node->num_children = (int32_t)in_node->children_count;
    for (j = 0; j < out_node->num_children; ++j)
      out_node->children[j] = (int32_t)(in_node->children[j] - gltf->nodes);

    if (in_node->name) {
      uint32_t const max_length = OWL_ARRAY_SIZE(out_node->name);
      OWL_STRNCPY(out_node->name, in_node->name, max_length);
    } else {
      uint32_t const max_length = OWL_ARRAY_SIZE(out_node->name);
      OWL_STRNCPY(out_node->name, "NO NAME", max_length);
    }

    if (in_node->has_translation) {
      out_node->translation[0] = in_node->translation[0];
      out_node->translation[1] = in_node->translation[1];
      out_node->translation[2] = in_node->translation[2];
    } else {
      out_node->translation[0] = 0.0F;
      out_node->translation[1] = 0.0F;
      out_node->translation[2] = 0.0F;
    }

    if (in_node->has_rotation) {
      out_node->rotation[0] = in_node->rotation[0];
      out_node->rotation[1] = in_node->rotation[1];
      out_node->rotation[2] = in_node->rotation[2];
      out_node->rotation[3] = in_node->rotation[3];
    } else {
      out_node->rotation[0] = 0.0F;
      out_node->rotation[1] = 0.0F;
      out_node->rotation[2] = 0.0F;
      out_node->rotation[3] = 0.0F;
    }

    if (in_node->has_scale) {
      out_node->scale[0] = in_node->scale[0];
      out_node->scale[1] = in_node->scale[1];
      out_node->scale[2] = in_node->scale[2];
    } else {
      out_node->scale[0] = 1.0F;
      out_node->scale[1] = 1.0F;
      out_node->scale[2] = 1.0F;
    }

    if (in_node->has_matrix)
      OWL_M4_COPY_V16(in_node->matrix, out_node->matrix);
    else
      OWL_M4_IDENTITY(out_node->matrix);

    if (in_node->skin)
      out_node->skin = (int32_t)(in_node->skin - gltf->skins);
    else
      out_node->skin = -1;

    /* FIXME(samuel): not sure if each node has it's own mesh, however as I
     * allocate resources per mesh, it's easier to give each one it's own
     * instead of checking if it exists */
    if (in_node->mesh) {
      struct cgltf_mesh const *in_mesh;
      struct owl_model_mesh *out_mesh;

      in_mesh = in_node->mesh;

#if 0
      out_node->mesh = (int32_t)(in_mesh - gltf->meshes);
#else
      out_node->mesh = m->num_meshes++;
#endif
      out_mesh = &m->meshes[out_node->mesh];

      OWL_ASSERT(in_mesh->primitives_count <
                 OWL_ARRAY_SIZE(out_mesh->primitives));

      out_mesh->num_primitives = (int32_t)in_mesh->primitives_count;
      for (j = 0; j < out_mesh->num_primitives; ++j) {
        int32_t k;
        int32_t has_skin;
        int32_t num_local_vertices = 0;
        int32_t num_local_indices = 0;

        int32_t position_stride = 0;
        float const *position = NULL;

        owl_v3 min_pos;
        owl_v3 max_pos;

        int32_t normal_stride = 0;
        float const *normal = NULL;

        int32_t uv0_stride = 0;
        float const *uv0 = NULL;

        int32_t uv1_stride = 0;
        float const *uv1 = NULL;

        int32_t joints0_component_is_u8;
        int32_t joints0_stride = 0;
        uint8_t const *joints0 = NULL;

        int32_t weights0_stride = 0;
        float const *weights0 = NULL;

        int32_t color0_stride = 0;
        float const *color0 = NULL;

        struct cgltf_attribute const *attr = NULL;
        struct owl_model_primitive *out_primitive = NULL;
        struct cgltf_primitive const *in_primitive = NULL;

        in_primitive = &in_mesh->primitives[j];
        out_mesh->primitives[j] = m->num_primitives;
        out_primitive = &m->primitives[m->num_primitives++];

        attr = owl_find_gltf_attribute(in_primitive, "POSITION");
        if (attr) {
          position = owl_resolve_gltf_accessor(attr->data);
          position_stride = owl_model_gltf_stride(attr->data->type);
          num_local_vertices = (int32_t)attr->data->count;
          if (attr->data->has_min) {
            min_pos[0] = attr->data->min[0];
            min_pos[1] = attr->data->min[1];
            min_pos[2] = attr->data->min[2];
          }

          if (attr->data->has_max) {
            max_pos[0] = attr->data->max[0];
            max_pos[1] = attr->data->max[1];
            max_pos[2] = attr->data->max[2];
          }
        }

        attr = owl_find_gltf_attribute(in_primitive, "NORMAL");
        if (attr) {
          normal = owl_resolve_gltf_accessor(attr->data);
          normal_stride = owl_model_gltf_stride(attr->data->type);
        }

        attr = owl_find_gltf_attribute(in_primitive, "TEXCOORD_0");
        if (attr) {
          uv0 = owl_resolve_gltf_accessor(attr->data);
          uv0_stride = owl_model_gltf_stride(attr->data->type);
        }

        attr = owl_find_gltf_attribute(in_primitive, "TEXCOORD_1");
        if (attr) {
          uv1 = owl_resolve_gltf_accessor(attr->data);
          uv1_stride = owl_model_gltf_stride(attr->data->type);
        }

        attr = owl_find_gltf_attribute(in_primitive, "JOINTS_0");
        if (attr) {
          joints0 = owl_resolve_gltf_accessor(attr->data);
          joints0_stride = owl_model_gltf_stride(attr->data->type);
          if (attr->data->component_type == cgltf_component_type_r_8u)
            joints0_component_is_u8 = 1;
          else
            joints0_component_is_u8 = 0;
        }

        attr = owl_find_gltf_attribute(in_primitive, "WEIGHTS_0");
        if (attr) {
          weights0 = owl_resolve_gltf_accessor(attr->data);
          weights0_stride = owl_model_gltf_stride(attr->data->type);
        }

        attr = owl_find_gltf_attribute(in_primitive, "COLOR_0");
        if (attr) {
          color0 = owl_resolve_gltf_accessor(attr->data);
          color0_stride = owl_model_gltf_stride(attr->data->type);
        }

        has_skin = joints0 && weights0;

        for (k = 0; k < num_local_vertices; ++k) {
          struct owl_model_vertex *vertex = &p->vertices[num_vertices + k];

          OWL_ASSERT(position);
          OWL_ASSERT(3 <= position_stride);
          vertex->position[0] = (&position[k * position_stride])[0];
          vertex->position[1] = (&position[k * position_stride])[1];
          vertex->position[2] = (&position[k * position_stride])[2];

          if (normal) {
            OWL_ASSERT(3 <= normal_stride);
            vertex->normal[0] = (&normal[k * normal_stride])[0];
            vertex->normal[1] = (&normal[k * normal_stride])[1];
            vertex->normal[2] = (&normal[k * normal_stride])[2];
          } else {
            vertex->normal[0] = 0.0F;
            vertex->normal[1] = 0.0F;
            vertex->normal[2] = 0.0F;
          }

          if (uv0) {
            OWL_ASSERT(2 <= uv0_stride);
            vertex->uv0[0] = (&uv0[k * uv0_stride])[0];
            vertex->uv0[1] = (&uv0[k * uv0_stride])[1];
          } else {
            vertex->uv0[0] = 0.0F;
            vertex->uv0[1] = 0.0F;
          }

          if (uv1) {
            OWL_ASSERT(2 <= uv1_stride);
            vertex->uv1[0] = (&uv1[k * uv1_stride])[0];
            vertex->uv1[1] = (&uv1[k * uv1_stride])[1];
          } else {
            vertex->uv1[0] = 0.0F;
            vertex->uv1[1] = 0.0F;
          }

          if (has_skin) {
            OWL_ASSERT(4 == joints0_stride);
            if (joints0_component_is_u8) {
              vertex->joints0[0] = (&joints0[k * joints0_stride])[0];
              vertex->joints0[1] = (&joints0[k * joints0_stride])[1];
              vertex->joints0[2] = (&joints0[k * joints0_stride])[2];
              vertex->joints0[3] = (&joints0[k * joints0_stride])[3];
            } else {
              uint16_t *joints0_u16 = (uint16_t *)joints0;
              vertex->joints0[0] = (&joints0_u16[k * joints0_stride])[0];
              vertex->joints0[1] = (&joints0_u16[k * joints0_stride])[1];
              vertex->joints0[2] = (&joints0_u16[k * joints0_stride])[2];
              vertex->joints0[3] = (&joints0_u16[k * joints0_stride])[3];
            }
          } else {
            vertex->joints0[0] = 0.0F;
            vertex->joints0[1] = 0.0F;
            vertex->joints0[2] = 0.0F;
            vertex->joints0[3] = 0.0F;
          }

          if (has_skin) {
            OWL_ASSERT(4 == weights0_stride);
            vertex->weights0[0] = (&weights0[k * weights0_stride])[0];
            vertex->weights0[1] = (&weights0[k * weights0_stride])[1];
            vertex->weights0[2] = (&weights0[k * weights0_stride])[2];
            vertex->weights0[3] = (&weights0[k * weights0_stride])[3];
          } else {
            vertex->weights0[0] = 1.0F;
            vertex->weights0[1] = 0.0F;
            vertex->weights0[2] = 0.0F;
            vertex->weights0[3] = 0.0F;
          }

          if (color0) {
            OWL_ASSERT(4 == color0_stride);
            vertex->color0[0] = (&color0[k * color0_stride])[0];
            vertex->color0[1] = (&color0[k * color0_stride])[1];
            vertex->color0[2] = (&color0[k * color0_stride])[2];
            vertex->color0[3] = (&color0[k * color0_stride])[3];
          } else {
            vertex->color0[0] = 1.0F;
            vertex->color0[1] = 1.0F;
            vertex->color0[2] = 1.0F;
            vertex->color0[3] = 1.0F;
          }
        }

        if (in_primitive->indices) {
          struct cgltf_accessor *accessor = in_primitive->indices;

          num_local_indices = (int32_t)accessor->count;

          OWL_ASSERT(num_indices + num_local_indices <= p->num_indices);

          switch (accessor->component_type) {
          case cgltf_component_type_r_32u: {
            int32_t l;
            uint32_t const *data = owl_resolve_gltf_accessor(accessor);
            for (l = 0; l < num_local_indices; ++l)
              p->indices[num_indices + l] = data[l] + num_vertices;
          } break;
          case cgltf_component_type_r_16u: {
            int32_t l;
            uint16_t const *data = owl_resolve_gltf_accessor(accessor);
            for (l = 0; l < num_local_indices; ++l)
              p->indices[num_indices + l] = data[l] + num_vertices;
          } break;
          case cgltf_component_type_r_8u: {
            int32_t l;
            uint8_t const *data = owl_resolve_gltf_accessor(accessor);
            for (l = 0; l < num_local_indices; ++l)
              p->indices[num_indices + l] = data[l] + num_vertices;
          } break;
          case cgltf_component_type_invalid:
          case cgltf_component_type_r_8:
          case cgltf_component_type_r_16:
          case cgltf_component_type_r_32f:
            OWL_ASSERT(0);
          }
        }

        {
          int32_t material;

          out_primitive->first = num_indices;
          out_primitive->num_indices = num_local_indices;
          out_primitive->num_vertices = num_local_vertices;
          out_primitive->has_indices = !!num_local_indices;
          out_primitive->bbox.valid = 0;

          out_primitive->bbox.min[0] = min_pos[0];
          out_primitive->bbox.min[1] = min_pos[1];
          out_primitive->bbox.min[2] = min_pos[2];

          out_primitive->bbox.max[0] = max_pos[0];
          out_primitive->bbox.max[1] = max_pos[1];
          out_primitive->bbox.max[2] = max_pos[2];

          material = (int32_t)(in_primitive->material - gltf->materials);
          out_primitive->material = material;

          num_indices += num_local_indices;
          num_vertices += num_local_vertices;
        }
      }

      for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(out_mesh->ssbos); ++j) {
        VkBufferCreateInfo info;
        VkResult vk_result;
        OWL_UNUSED(vk_result);

        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.size = sizeof(struct owl_model_joints_ssbo);
        info.usage = 0;
        info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = NULL;

        vk_result = vkCreateBuffer(device, &info, NULL, &out_mesh->ssbos[j]);
        OWL_ASSERT(!vk_result);
      }

      {
        uint64_t aligned_size;
        VkMemoryPropertyFlagBits properties;
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo info;
        VkResult vk_result;
        OWL_UNUSED(vk_result);

        properties = 0;
        properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        vkGetBufferMemoryRequirements(device, out_mesh->ssbos[0],
                                      &requirements);

        aligned_size = OWL_ALIGN_UP_2(requirements.size,
                                      requirements.alignment);

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = aligned_size * OWL_ARRAY_SIZE(out_mesh->ssbos);
        info.memoryTypeIndex = owl_renderer_find_memory_type(
            r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL,
                                     &out_mesh->ssbo_memory);
        OWL_ASSERT(!vk_result);

        for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(out_mesh->ssbos); ++j) {
          vk_result = vkBindBufferMemory(device, out_mesh->ssbos[j],
                                         out_mesh->ssbo_memory,
                                         j * aligned_size);
          OWL_ASSERT(!vk_result);
        }

        {
          void *data;
          vk_result = vkMapMemory(device, out_mesh->ssbo_memory, 0,
                                  VK_WHOLE_SIZE, 0, &data);
          OWL_ASSERT(!vk_result);
          for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(out_mesh->ssbos); ++j) {
            uint64_t const offset = j * aligned_size;
            out_mesh->mapped_ssbos[j] = (void *)&((uint8_t *)(data))[offset];
          }
        }

        for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(out_mesh->ssbos); ++j) {
          int32_t l;
          struct owl_model_joints_ssbo *ssbo = out_mesh->mapped_ssbos[j];

          if (-1 != out_node->skin)
            ssbo->num_joints = m->skins[out_node->skin].num_joints;
          else
            ssbo->num_joints = 0;

          OWL_M4_IDENTITY(ssbo->matrix);

          for (l = 0; l < (int32_t)OWL_ARRAY_SIZE(ssbo->joints); ++l)
            OWL_M4_IDENTITY(ssbo->joints[l]);
        }
      }

      {
        VkDescriptorSetLayout layouts[OWL_ARRAY_SIZE(out_mesh->ssbos)];
        VkDescriptorSetAllocateInfo info;
        VkResult vk_result;
        OWL_UNUSED(vk_result);

        OWL_ASSERT(OWL_ARRAY_SIZE(out_mesh->ssbo_descriptor_sets) ==
                   OWL_ARRAY_SIZE(out_mesh->ssbos));

        for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(layouts); ++j)
          layouts[j] = r->model_storage_descriptor_set_layout;

        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.pNext = NULL;
        info.descriptorPool = r->descriptor_pool;
        info.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
        info.pSetLayouts = layouts;

        vk_result = vkAllocateDescriptorSets(device, &info,
                                             out_mesh->ssbo_descriptor_sets);
        OWL_ASSERT(!vk_result);
      }
      {
        VkDescriptorBufferInfo descriptors[OWL_ARRAY_SIZE(out_mesh->ssbos)];
        VkWriteDescriptorSet writes[OWL_ARRAY_SIZE(out_mesh->ssbos)];

        for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(descriptors); ++j) {
          descriptors[j].buffer = out_mesh->ssbos[j];
          descriptors[j].offset = 0;
          descriptors[j].range = sizeof(struct owl_model_joints_ssbo);

          writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
          writes[j].pNext = NULL;
          writes[j].dstSet = out_mesh->ssbo_descriptor_sets[j];
          writes[j].dstBinding = 0;
          writes[j].dstArrayElement = 0;
          writes[j].descriptorCount = 1;
          writes[j].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          writes[j].pImageInfo = NULL;
          writes[j].pBufferInfo = &descriptors[j];
          writes[j].pTexelBufferView = NULL;
        }

        vkUpdateDescriptorSets(device, OWL_ARRAY_SIZE(writes), writes, 0,
                               NULL);
      }
    } else {
      out_node->mesh = -1;
    }
  }

  OWL_ASSERT(num_indices == p->num_indices);
  OWL_ASSERT(num_vertices == p->num_vertices);

  return ret;
}

static void owl_model_unload_nodes(struct owl_renderer *r,
                                   struct owl_model *m) {
  int32_t i;
  VkDevice const device = r->device;
  /* could just iterate each mesh instead of going trough each node,
     however it's easier to cleanup future resource allocations this way */
  for (i = 0; i < m->num_nodes; ++i) {
    struct owl_model_node *node = &m->nodes[i];

    if (-1 != node->mesh) {
      int32_t j;
      struct owl_model_mesh *mesh = &m->meshes[node->mesh];
      vkFreeDescriptorSets(device, r->descriptor_pool,
                           OWL_ARRAY_SIZE(mesh->ssbos),
                           mesh->ssbo_descriptor_sets);

      vkFreeMemory(device, mesh->ssbo_memory, NULL);

      for (j = 0; j < (int32_t)OWL_ARRAY_SIZE(mesh->ssbos); ++j)
        vkDestroyBuffer(device, mesh->ssbos[j], NULL);
    }
  }
}

static int owl_model_init_buffers(struct owl_renderer *r,
                                  struct owl_model_all_primitives *p,
                                  struct owl_model *m) {
  int ret = OWL_OK;
  VkDevice const device = r->device;

  {
    VkBufferCreateInfo info;
    VkResult vk_result;
    OWL_UNUSED(vk_result);

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = p->num_vertices * sizeof(*p->vertices);
    info.usage = 0;
    info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = 0;

    vk_result = vkCreateBuffer(device, &info, NULL, &m->vertex_buffer);
    OWL_ASSERT(!vk_result);
  }

  {
    VkMemoryPropertyFlagBits properties;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;
    VkResult vk_result;
    OWL_UNUSED(vk_result);

    properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetBufferMemoryRequirements(device, m->vertex_buffer, &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL, &m->vertex_memory);
    OWL_ASSERT(!vk_result);

    vk_result = vkBindBufferMemory(device, m->vertex_buffer, m->vertex_memory,
                                   0);
    OWL_ASSERT(!vk_result);
  }

  {
    VkBufferCopy copy;
    uint64_t size;
    void *data;
    struct owl_renderer_upload_allocation allocation;

    ret = owl_renderer_begin_im_command_buffer(r);
    OWL_ASSERT(!ret);

    size = p->num_vertices * sizeof(*p->vertices);
    data = owl_renderer_upload_allocate(r, size, &allocation);
    OWL_ASSERT(data);

    OWL_MEMCPY(data, p->vertices, size);

#if 0
    {
      int32_t i;
      OWL_DEBUG_LOG("splating vertices...\n");
      for (i = 0; i < p->num_vertices; ++i) {
        struct owl_model_vertex *vertex = &p->vertices[i];
        OWL_DEBUG_LOG("  p->vertices[%i]:\n", i);
        OWL_DEBUG_LOG("    position: " OWL_V3_FORMAT "\n",
                      OWL_V3_FORMAT_ARGS(vertex->position));
        OWL_DEBUG_LOG("    normal: " OWL_V3_FORMAT "\n",
                      OWL_V3_FORMAT_ARGS(vertex->normal));
        OWL_DEBUG_LOG("    uv0: " OWL_V2_FORMAT "\n",
                      OWL_V2_FORMAT_ARGS(vertex->uv0));
        OWL_DEBUG_LOG("    uv1: " OWL_V2_FORMAT "\n",
                      OWL_V2_FORMAT_ARGS(vertex->uv1));
        OWL_DEBUG_LOG("    joints0: " OWL_V4_FORMAT "\n",
                      OWL_V4_FORMAT_ARGS(vertex->joints0));
        OWL_DEBUG_LOG("    weights0: " OWL_V4_FORMAT "\n",
                      OWL_V4_FORMAT_ARGS(vertex->weights0));
        OWL_DEBUG_LOG("    color0: " OWL_V4_FORMAT "\n",
                      OWL_V4_FORMAT_ARGS(vertex->color0));
      }
    }
#endif

    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;

    vkCmdCopyBuffer(r->im_command_buffer, allocation.buffer, m->vertex_buffer,
                    1, &copy);

    ret = owl_renderer_end_im_command_buffer(r);
    OWL_ASSERT(!ret);

    owl_renderer_upload_free(r, data);
  }

  if (p->num_indices) {
    {
      VkBufferCreateInfo info;
      VkResult vk_result;
      OWL_UNUSED(vk_result);

      info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.size = p->num_indices * sizeof(*p->indices);
      info.usage = 0;
      info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
      info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = 0;

      vk_result = vkCreateBuffer(device, &info, NULL, &m->index_buffer);
      OWL_ASSERT(!vk_result);
    }

    {
      VkMemoryPropertyFlagBits properties;
      VkMemoryRequirements requirements;
      VkMemoryAllocateInfo info;
      VkResult vk_result;
      OWL_UNUSED(vk_result);

      properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

      vkGetBufferMemoryRequirements(device, m->index_buffer, &requirements);

      info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      info.pNext = NULL;
      info.allocationSize = requirements.size;
      info.memoryTypeIndex = owl_renderer_find_memory_type(
          r, requirements.memoryTypeBits, properties);

      vk_result = vkAllocateMemory(device, &info, NULL, &m->index_memory);
      OWL_ASSERT(!vk_result);

      vk_result = vkBindBufferMemory(device, m->index_buffer, m->index_memory,
                                     0);
      OWL_ASSERT(!vk_result);
    }

    {
      VkBufferCopy copy;
      uint64_t size;
      void *data;
      struct owl_renderer_upload_allocation allocation;

      ret = owl_renderer_begin_im_command_buffer(r);
      OWL_ASSERT(!ret);

      size = p->num_indices * sizeof(*p->indices);
      data = owl_renderer_upload_allocate(r, size, &allocation);
      OWL_ASSERT(data);

      OWL_MEMCPY(data, p->indices, size);

#if 0
      {
        int32_t i;
        OWL_DEBUG_LOG("splating indices...\n");
        for (i = 0; i < p->num_indices; ++i) {
          OWL_DEBUG_LOG("  p->indices[%i]: = %u\n", i, p->indices[i]);
          OWL_ASSERT(p->indices[i] <= (uint32_t)p->num_vertices);
        }
      }
#endif

      copy.srcOffset = 0;
      copy.dstOffset = 0;
      copy.size = size;

      vkCmdCopyBuffer(r->im_command_buffer, allocation.buffer, m->index_buffer,
                      1, &copy);

      ret = owl_renderer_end_im_command_buffer(r);
      OWL_ASSERT(!ret);

      owl_renderer_upload_free(r, data);
    }

    m->has_indices = 1;
  } else {
    m->index_buffer = VK_NULL_HANDLE;
    m->index_memory = VK_NULL_HANDLE;
    m->has_indices = 0;
  }

  return ret;
}

static void owl_model_deinit_buffers(struct owl_renderer *r,
                                     struct owl_model *m) {
  VkDevice const device = r->device;

  if (m->has_indices) {
    vkFreeMemory(device, m->index_memory, NULL);
    vkDestroyBuffer(device, m->index_buffer, NULL);
  }

  vkFreeMemory(device, m->vertex_memory, NULL);
  vkDestroyBuffer(device, m->vertex_buffer, NULL);
}

/* TODO(samuel): do a simplify pass */
static int owl_model_load_skins(struct owl_renderer *r,
                                struct cgltf_data const *gltf,
                                struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;

  OWL_UNUSED(r);

  OWL_ASSERT(gltf->skins_count < OWL_ARRAY_SIZE(m->skins));

  m->num_skins = (int32_t)gltf->skins_count;
  for (i = 0; i < m->num_skins; ++i) {
    int32_t j;
    struct owl_model_skin *out_skin = &m->skins[i];
    struct cgltf_skin const *in_skin = &gltf->skins[i];

    if (in_skin->name) {
      uint32_t max_length = OWL_ARRAY_SIZE(out_skin->name);
      OWL_STRNCPY(out_skin->name, in_skin->name, max_length);
    } else {
      uint32_t max_length = OWL_ARRAY_SIZE(out_skin->name);
      OWL_STRNCPY(out_skin->name, "NO NAME", max_length);
    }

    if (in_skin->skeleton)
      out_skin->root = (int32_t)(in_skin->skeleton - gltf->nodes);
    else
      out_skin->root = -1;

    OWL_ASSERT(in_skin->joints_count < OWL_ARRAY_SIZE(out_skin->joints));

    out_skin->num_joints = (int32_t)in_skin->joints_count;
    for (j = 0; j < out_skin->num_joints; ++j)
      out_skin->joints[j] = (int32_t)(in_skin->joints[j] - gltf->nodes);

    if (in_skin->inverse_bind_matrices) {
      struct cgltf_accessor *accessor = in_skin->inverse_bind_matrices;
      owl_m4 const *matrices = owl_resolve_gltf_accessor(accessor);

      OWL_ASSERT(accessor->count < OWL_ARRAY_SIZE(out_skin->joints));

      out_skin->num_inverse_bind_matrices = accessor->count;
      for (j = 0; j < out_skin->num_inverse_bind_matrices; ++j)
        OWL_M4_COPY(matrices[j], out_skin->inverse_bind_matrices[j]);
    }

    /* allocate and write the descriptor sets */
  }

  return ret;
}

static void owl_model_unload_skins(struct owl_renderer *r,
                                   struct owl_model *m) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
}

static int owl_model_load_animations(struct owl_renderer *r,
                                     struct cgltf_data const *gltf,
                                     struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;

  OWL_UNUSED(r);

  OWL_ASSERT(OWL_ANIMATION_INTERPOLATION_LINEAR ==
             cgltf_interpolation_type_linear);
  OWL_ASSERT(OWL_ANIMATION_INTERPOLATION_STEP ==
             cgltf_interpolation_type_step);
  OWL_ASSERT(OWL_ANIMATION_INTERPOLATION_CUBICSPLINE ==
             cgltf_interpolation_type_cubic_spline);

  OWL_ASSERT(OWL_ANIMATION_PATH_TRANSLATION ==
             cgltf_animation_path_type_translation);
  OWL_ASSERT(OWL_ANIMATION_PATH_ROTATION ==
             cgltf_animation_path_type_rotation);
  OWL_ASSERT(OWL_ANIMATION_PATH_SCALE == /*   */
             cgltf_animation_path_type_scale);
  OWL_ASSERT(OWL_ANIMATION_PATH_WEIGHTS == /*  */
             cgltf_animation_path_type_weights);

  OWL_ASSERT(gltf->animations_count < OWL_ARRAY_SIZE(m->animations));

  m->num_samplers = 0;
  m->num_channels = 0;

  m->num_animations = (int32_t)gltf->animations_count;
  for (i = 0; i < m->num_animations; ++i) {
    int32_t j;
    struct owl_model_animation *out_animation = &m->animations[i];
    struct cgltf_animation const *in_animation = &gltf->animations[i];

    out_animation->time = 0.0F;
    out_animation->start = FLT_MAX;
    out_animation->end = FLT_MIN;

    if (in_animation->name) {
      int32_t max_length = OWL_ARRAY_SIZE(out_animation->name);
      OWL_STRNCPY(out_animation->name, in_animation->name, max_length);
    } else {
      int32_t max_length = OWL_ARRAY_SIZE(out_animation->name);
      OWL_STRNCPY(out_animation->name, "NO NAME", max_length);
    }

    OWL_ASSERT(in_animation->samplers_count <
               OWL_ARRAY_SIZE(out_animation->samplers));

    out_animation->num_samplers = (int32_t)in_animation->samplers_count;
    for (j = 0; j < out_animation->num_samplers; ++j) {
      int32_t k;
      float const *inputs;
      struct owl_model_animation_sampler *out_sampler;
      struct cgltf_animation_sampler *in_sampler;

      out_animation->samplers[j] = m->num_samplers + j;

      in_sampler = &in_animation->samplers[j];
      out_sampler = &m->samplers[out_animation->samplers[j]];

      out_sampler->interpolation = in_sampler->interpolation;

      /* FIXME(samuel): validate this component type */
      OWL_ASSERT(in_sampler->input);
      inputs = owl_resolve_gltf_accessor(in_sampler->input);

      OWL_ASSERT(in_sampler->input->count <
                 OWL_ARRAY_SIZE(out_sampler->inputs));

      out_sampler->num_inputs = (int32_t)in_sampler->input->count;
      for (k = 0; k < out_sampler->num_inputs; ++k) {
        float const input = inputs[k];

        if (input < out_animation->start)
          out_animation->start = input;

        if (input > out_animation->end)
          out_animation->end = input;

        out_sampler->inputs[k] = input;
      }

      OWL_ASSERT(in_sampler->output->count <
                 OWL_ARRAY_SIZE(out_sampler->outputs));

      out_sampler->num_outputs = (int32_t)in_sampler->output->count;
      switch (in_sampler->output->type) {
      case cgltf_type_vec3: {
        owl_v3 const *outputs;
        outputs = owl_resolve_gltf_accessor(in_sampler->output);

        for (k = 0; k < out_sampler->num_outputs; ++k) {
          OWL_V4_ZERO(out_sampler->outputs[k]);
          OWL_V3_COPY(outputs[k], out_sampler->outputs[k]);
        }
      } break;

      case cgltf_type_vec4: {
        owl_v4 const *outputs;
        outputs = owl_resolve_gltf_accessor(in_sampler->output);
        for (k = 0; k < out_sampler->num_outputs; ++k) {
          OWL_V4_COPY(outputs[k], out_sampler->outputs[k]);
        }
      } break;

      case cgltf_type_invalid:
      case cgltf_type_scalar:
      case cgltf_type_vec2:
      case cgltf_type_mat2:
      case cgltf_type_mat3:
      case cgltf_type_mat4:
        OWL_ASSERT(0);
      }
    }

    m->num_samplers += out_animation->num_samplers;

    OWL_ASSERT(in_animation->channels_count <
               OWL_ARRAY_SIZE(out_animation->channels));

    out_animation->num_channels = in_animation->channels_count;
    for (j = 0; j < out_animation->num_channels; ++j) {
      int32_t id;
      struct owl_model_animation_channel *out_channel;
      struct cgltf_animation_channel *in_channel;

      out_animation->channels[j] = m->num_channels + j;

      in_channel = &in_animation->channels[j];
      out_channel = &m->channels[out_animation->channels[j]];

      out_channel->path = in_channel->target_path;
      out_channel->node = (int32_t)(in_channel->target_node - gltf->nodes);

      id = (int32_t)(in_channel->sampler - in_animation->samplers);
      out_channel->sampler = out_animation->samplers[id];
    }

    m->num_channels += out_animation->num_channels;
  }

  return ret;
}

static void owl_model_unload_animations(struct owl_renderer *r,
                                        struct owl_model *m) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
}

static int owl_model_load_roots(struct owl_renderer *r,
                                struct cgltf_data const *gltf,
                                struct owl_model *m) {
  int32_t i;
  int ret = OWL_OK;
  struct cgltf_scene const *in_scene = gltf->scene;

  OWL_UNUSED(r);

  OWL_ASSERT(in_scene->nodes_count < OWL_ARRAY_SIZE(m->roots));

  m->num_roots = in_scene->nodes_count;
  for (i = 0; i < m->num_roots; ++i)
    m->roots[i] = (int32_t)(in_scene->nodes[i] - gltf->nodes);

  return ret;
}

static void owl_model_unload_roots(struct owl_renderer *r,
                                   struct owl_model *m) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
}

#define OWL_PATH_SEPARATOR '/'

/* TODO(samuel): cleanup on error */
OWLAPI int owl_model_init(struct owl_model *model, struct owl_renderer *r,
                          char const *path) {
  struct owl_texture_desc empty_desc;
  struct cgltf_options options;
  struct cgltf_data *data = NULL;
  struct owl_model_all_primitives all_primitives;

  int ret = OWL_OK;

  OWL_MEMSET(&options, 0, sizeof(options));
  OWL_MEMSET(model, 0, sizeof(*model));
  OWL_MEMSET(&empty_desc, 0, sizeof(empty_desc));

  empty_desc.source = OWL_TEXTURE_SOURCE_FILE;
  empty_desc.path = "../../assets/none.png";
  ret = owl_texture_init(r, &empty_desc, &model->empty_texture);
  OWL_ASSERT(!ret);

  OWL_STRNCPY(model->path, path, sizeof(model->path));

  {
    uint32_t end = 0;
    uint64_t const max_length = sizeof(model->directory);

    OWL_STRNCPY(model->directory, path, sizeof(model->path));

    while ('\0' != model->directory[end] && end < max_length)
      ++end;

    while (OWL_PATH_SEPARATOR != model->directory[end] && end)
      --end;

    model->directory[end] = '\0';
  }

  if (cgltf_result_success != cgltf_parse_file(&options, path, &data)) {
    OWL_DEBUG_LOG("Filed to parse gltf file!");
    ret = OWL_ERROR_FATAL;
  }

  if (cgltf_result_success != cgltf_load_buffers(&options, data, path)) {
    OWL_DEBUG_LOG("Filed to parse load gltf buffers!");
    ret = OWL_ERROR_FATAL;
    goto out;
  }

  ret = owl_model_load_images(r, data, model);
  OWL_ASSERT(!ret);

  ret = owl_model_load_textures(r, data, model);
  OWL_ASSERT(!ret);

  ret = owl_model_load_materials(r, data, model);
  OWL_ASSERT(!ret);

  ret = owl_model_init_all_primitives(&all_primitives, data);
  OWL_ASSERT(!ret);

  ret = owl_model_load_skins(r, data, model);
  OWL_ASSERT(!ret);

  ret = owl_model_load_nodes(r, data, &all_primitives, model);
  OWL_ASSERT(!ret);

  ret = owl_model_init_buffers(r, &all_primitives, model);
  OWL_ASSERT(!ret);

  ret = owl_model_load_animations(r, data, model);
  OWL_ASSERT(!ret);

  ret = owl_model_load_roots(r, data, model);
  OWL_ASSERT(!ret);

  owl_model_deinit_all_primitives(&all_primitives);

  cgltf_free(data);

out:
  return ret;
}

OWLAPI void owl_model_deinit(struct owl_model *model, struct owl_renderer *r) {
  vkDeviceWaitIdle(r->device);
  owl_model_unload_roots(r, model);
  owl_model_unload_animations(r, model);
  owl_model_unload_skins(r, model);
  owl_model_deinit_buffers(r, model);
  owl_model_unload_nodes(r, model);
  owl_model_unload_materials(r, model);
  owl_model_unload_textures(r, model);
  owl_model_unload_images(r, model);
  owl_texture_deinit(r, &model->empty_texture);
}

static void owl_model_resolve_local_node_matrix(struct owl_model const *m,
                                                int32_t id, owl_m4 matrix) {
  owl_m4 tmp;
  struct owl_model_node const *node = &m->nodes[id];

  OWL_M4_IDENTITY(matrix);
  owl_m4_translate(node->translation, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_q4_as_m4(node->rotation, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  OWL_M4_IDENTITY(tmp);
  owl_m4_scale_v3(tmp, node->scale, tmp);
  owl_m4_multiply(matrix, tmp, matrix);

  owl_m4_multiply(matrix, node->matrix, matrix);
}

static void owl_model_resolve_node_matrix(struct owl_model const *m,
                                          int32_t id, owl_m4 matrix) {
  int32_t p;

  owl_model_resolve_local_node_matrix(m, id, matrix);

  for (p = m->nodes[id].parent; - 1 != p; p = m->nodes[p].parent) {
    owl_m4 local;
    owl_model_resolve_local_node_matrix(m, p, local);
    owl_m4_multiply(local, matrix, matrix);
  }
}

static void owl_model_update_node_joints(struct owl_renderer *r,
                                         struct owl_model *m, int32_t id) {
  int32_t i;
  owl_m4 tmp;
  owl_m4 inverse;
  struct owl_model_mesh const *mesh;
  struct owl_model_skin const *skin;
  struct owl_model_node const *node;
  uint32_t const frame = r->frame;

  node = &m->nodes[id];

  for (i = 0; i < node->num_children; ++i)
    owl_model_update_node_joints(r, m, node->children[i]);

  if (-1 == node->mesh)
    return;

  if (-1 == node->skin)
    return;

  mesh = &m->meshes[node->mesh];
  skin = &m->skins[node->skin];

  owl_model_resolve_node_matrix(m, id, tmp);
  owl_m4_inverse(tmp, inverse);

#if 1
  for (i = 0; i < skin->num_joints; ++i) {
    struct owl_model_joints_ssbo *ssbo = mesh->mapped_ssbos[frame];

    owl_model_resolve_node_matrix(m, skin->joints[i], tmp);
    owl_m4_multiply(tmp, skin->inverse_bind_matrices[i], tmp);
    owl_m4_multiply(inverse, tmp, ssbo->joints[i]);
  }
#else
  OWL_UNUSED(frame);
  OWL_UNUSED(skin);
#endif
}

OWLAPI int owl_model_update_animation(struct owl_renderer *r,
                                      struct owl_model *m, float dt,
                                      int32_t id) {
  int32_t i;
  struct owl_model_animation *animation;

  if (-1 >= id || id >= (int32_t)OWL_ARRAY_SIZE(m->animations))
    return OWL_ERROR_FATAL; /* TODO(samuel0) invalid value */

  animation = &m->animations[id];

  if (animation->end < (animation->time += dt))
    animation->time -= animation->end;

  for (i = 0; i < animation->num_channels; ++i) {
    int32_t j;
    int32_t const channel_id = animation->channels[i];
    struct owl_model_animation_channel *channel = &m->channels[channel_id];
    int32_t const sampler_id = channel->sampler;
    struct owl_model_animation_sampler *sampler = &m->samplers[sampler_id];
    struct owl_model_node *node = &m->nodes[channel->node];

    if (OWL_ANIMATION_INTERPOLATION_LINEAR != sampler->interpolation)
      continue;

    for (j = 0; j < sampler->num_inputs - 1; ++j) {
      float const i0 = sampler->inputs[j];
      float const i1 = sampler->inputs[j + 1];
      float const time = animation->time;
      float const a = (time - i0) / (i1 - i0);

      if (!((time >= i0) && (time <= i1)))
        continue;

      switch (channel->path) {
      case OWL_ANIMATION_PATH_TRANSLATION: {
        owl_v3_mix(sampler->outputs[j], sampler->outputs[j + 1], a,
                   node->translation);
      } break;

      case OWL_ANIMATION_PATH_ROTATION: {
        owl_v4_quat_slerp(sampler->outputs[j], sampler->outputs[j + 1], a,
                          node->rotation);
        owl_v4_normalize(node->rotation, node->rotation);
      } break;

      case OWL_ANIMATION_PATH_SCALE: {
        owl_v3_mix(sampler->outputs[j], sampler->outputs[j + 1], a,
                   node->scale);
      } break;

      default:
        OWL_ASSERT(0 && "unexpected path");
        return OWL_ERROR_FATAL;
      }
    }
  }

  for (i = 0; i < m->num_roots; ++i)
    owl_model_update_node_joints(r, m, m->roots[i]);

  return OWL_OK;
}

#ifndef OWL_MODEL_H_
#define OWL_MODEL_H_

#include "owl-definitions.h"
#include "owl-vk-renderer.h"
#include "owl-vk-texture.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

#define OWL_MODEL_MESH_NONE -1
#define OWL_MODEL_ANIM_NONE -1
#define OWL_MODEL_SKIN_NONE -1
#define OWL_MODEL_NODE_NONE -1
#define OWL_MODEL_TEXTURE_NONE -1
#define OWL_MODEL_MAX_ITEMS 128
#define OWL_MODEL_MAX_NAME_LENGTH 128
#define OWL_MODEL_MAX_JOINTS 128

typedef int32_t owl_model_node_id;
typedef int32_t owl_model_mesh_id;
typedef int32_t owl_model_texture_id;
typedef int32_t owl_model_material_id;
typedef int32_t owl_model_primitive_id;
typedef int32_t owl_model_image_id;
typedef int32_t owl_model_skin_id;
typedef int32_t owl_model_anim_sampler_id;
typedef int32_t owl_model_anim_chan_id;
typedef int32_t owl_model_anim_id;

struct owl_model_material_push_constant {
  owl_v4 base_color_factor;
  owl_v4 emissive_factor;
  owl_v4 diffuse_factor;
  owl_v4 specular_factor;
  float workflow;
  int32_t base_color_uv_set;
  int32_t physical_desc_uv_set;
  int32_t normal_uv_set;
  int32_t occlusion_uv_set;
  int32_t emissive_uv_set;
  float metallic_factor;
  float roughness_factor;
  float alpha_mask;
  float alpha_mask_cutoff;
};

struct owl_model_ubo1 {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
  owl_v4 light;
};

struct owl_model_mesh {
  int32_t primitive_count;
  owl_model_primitive_id primitives[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_ubo2 {
  owl_v4 light_direction;
};

struct owl_model_primitive {
  uint32_t first;
  uint32_t count;
  owl_model_material_id material;
};

struct owl_model_node {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;
  owl_m4 matrix;

  owl_model_node_id parent;
  owl_model_mesh_id mesh;
  owl_model_skin_id skin;

  int32_t child_count;
  owl_model_node_id children[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_image {
  struct owl_vk_texture image;
};

struct owl_model_texture {
  owl_model_image_id image;
};

enum owl_model_alpha_mode {
  OWL_MODEL_ALPHA_MODE_OPAQUE,
  OWL_MODEL_ALPHA_MODE_MASK,
  OWL_MODEL_ALPHA_MODE_BLEND
};

struct owl_model_material {
  enum owl_model_alpha_mode alpha_mode;
  float alpha_cutoff;
  int32_t double_sided;
  owl_model_texture_id base_color_texture;
  owl_model_texture_id normal_texture;
  owl_model_texture_id physical_desc_texture;
  owl_model_texture_id occlusion_texture;
  owl_model_texture_id emissive_texture;
  owl_v4 base_color_factor;
};

struct owl_model_skin_ssbo {
  owl_m4 matrix;
  owl_m4 joint_matices[OWL_MODEL_MAX_JOINTS];
  int32_t joint_matrix_count;
};

struct owl_model_skin {
  char name[OWL_MODEL_MAX_NAME_LENGTH];
  owl_model_node_id skeleton_root;

  VkBuffer ssbo_buffers[OWL_MODEL_MAX_ITEMS];
  VkDescriptorSet ssbo_sets[OWL_MODEL_MAX_ITEMS];

  owl_m4 inverse_bind_matrices[OWL_MODEL_MAX_JOINTS];

  int32_t joint_count;
  owl_model_node_id joints[OWL_MODEL_MAX_JOINTS];

  VkDeviceMemory ssbo_memory;

  uint64_t ssbo_buffer_size;
  uint64_t ssbo_buffer_alignment;
  uint64_t ssbo_buffer_aligned_size;

  /* NOTE(samuel): directly mapped skin joint data */
  struct owl_model_skin_ssbo *ssbos[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_anim_sampler {
  int32_t interpolation;

  int32_t input_count;
  float inputs[OWL_MODEL_MAX_ITEMS];

  int32_t output_count;
  owl_v4 outputs[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_anim_chan {
  int32_t path;

  owl_model_node_id node;
  owl_model_anim_sampler_id anim_sampler;
};

struct owl_model_anim {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  float current_time;
  float begin;
  float end;

  int32_t sampler_count;
  owl_model_anim_sampler_id samplers[OWL_MODEL_MAX_ITEMS];

  int32_t chan_count;
  owl_model_anim_chan_id chans[OWL_MODEL_MAX_ITEMS];
};

struct owl_model {
  VkBuffer vk_vertex_buffer;
  VkDeviceMemory vk_vertex_memory;

  VkBuffer vk_index_buffer;
  VkDeviceMemory vk_index_memory;

  owl_model_anim_id active_anim;

  int32_t root_count;
  owl_model_node_id roots[OWL_MODEL_MAX_ITEMS];

  int32_t node_count;
  struct owl_model_node nodes[OWL_MODEL_MAX_ITEMS];

  int32_t image_count;
  struct owl_model_image images[OWL_MODEL_MAX_ITEMS];

  int32_t texture_count;
  struct owl_model_texture textures[OWL_MODEL_MAX_ITEMS];

  int32_t material_count;
  struct owl_model_material materials[OWL_MODEL_MAX_ITEMS];

  int32_t mesh_count;
  struct owl_model_mesh meshes[OWL_MODEL_MAX_ITEMS];

  int32_t primitive_count;
  struct owl_model_primitive primitives[OWL_MODEL_MAX_ITEMS];

  int32_t skin_count;
  struct owl_model_skin skins[OWL_MODEL_MAX_ITEMS];

  int32_t anim_sampler_count;
  struct owl_model_anim_sampler anim_samplers[OWL_MODEL_MAX_ITEMS];

  int32_t anim_chan_count;
  struct owl_model_anim_chan anim_chans[OWL_MODEL_MAX_ITEMS];

  int32_t anim_count;
  struct owl_model_anim anims[OWL_MODEL_MAX_ITEMS];
};

owl_public owl_code
owl_model_init(struct owl_model *model, struct owl_vk_renderer *vk,
               char const *path);

owl_public void
owl_model_deinit(struct owl_model *model, struct owl_vk_renderer *vk);

owl_public owl_code
owl_model_anim_update(struct owl_model *model, int32_t frame, float dt,
                      owl_model_anim_id id);

OWL_END_DECLS

#endif

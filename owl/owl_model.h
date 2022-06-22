#ifndef OWL_MODEL_H_
#define OWL_MODEL_H_

#include "owl_definitions.h"
#include "owl_renderer.h"
#include "owl_texture_2d.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

#define OWL_MODEL_MESH_NONE -1
#define OWL_MODEL_ANIM_NONE -1
#define OWL_MODEL_SKIN_NONE -1
#define OWL_MODEL_NODE_NONE -1
#define OWL_MODEL_TEXTURE_NONE -1
#define OWL_MODEL_MAX_ITEMS 128
#define OWL_MODEL_MAX_NAME_LENGTH 128
#define OWL_MODEL_MAX_JOINTS 128

typedef int owl_model_node_id;
typedef int owl_model_mesh_id;
typedef int owl_model_texture_id;
typedef int owl_model_material_id;
typedef int owl_model_primitive_id;
typedef int owl_model_image_id;
typedef int owl_model_skin_id;
typedef int owl_model_anim_sampler_id;
typedef int owl_model_anim_chan_id;
typedef int owl_model_anim_id;

struct owl_model_material_push_constant {
  owl_v4 base_color_factor;
  owl_v4 emissive_factor;
  owl_v4 diffuse_factor;
  owl_v4 specular_factor;
  float workflow;
  int base_color_uv_set;
  int physical_desc_uv_set;
  int normal_uv_set;
  int occlusion_uv_set;
  int emissive_uv_set;
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
  int primitive_count;
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

  int child_count;
  owl_model_node_id children[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_image {
  struct owl_texture_2d image;
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
  int double_sided;
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
  int joint_matrix_count;
};

struct owl_model_skin {
  char name[OWL_MODEL_MAX_NAME_LENGTH];
  owl_model_node_id skeleton_root;

  VkBuffer ssbo_buffers[OWL_MODEL_MAX_ITEMS];
  VkDescriptorSet ssbo_sets[OWL_MODEL_MAX_ITEMS];

  owl_m4 inverse_bind_matrices[OWL_MODEL_MAX_JOINTS];

  int joint_count;
  owl_model_node_id joints[OWL_MODEL_MAX_JOINTS];

  VkDeviceMemory ssbo_memory;

  uint64_t ssbo_buffer_size;
  uint64_t ssbo_buffer_alignment;
  uint64_t ssbo_buffer_aligned_size;

  /* NOTE(samuel): directly mapped skin joint data */
  struct owl_model_skin_ssbo *ssbos[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_anim_sampler {
  int interpolation;

  int input_count;
  float inputs[OWL_MODEL_MAX_ITEMS];

  int output_count;
  owl_v4 outputs[OWL_MODEL_MAX_ITEMS];
};

struct owl_model_anim_chan {
  int path;

  owl_model_node_id node;
  owl_model_anim_sampler_id anim_sampler;
};

struct owl_model_anim {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  float current_time;
  float begin;
  float end;

  int sampler_count;
  owl_model_anim_sampler_id samplers[OWL_MODEL_MAX_ITEMS];

  int chan_count;
  owl_model_anim_chan_id chans[OWL_MODEL_MAX_ITEMS];
};

struct owl_model {
  VkBuffer vk_vertex_buffer;
  VkDeviceMemory vk_vertex_memory;

  VkBuffer vk_index_buffer;
  VkDeviceMemory vk_index_memory;

  owl_model_anim_id active_anim;

  int root_count;
  owl_model_node_id roots[OWL_MODEL_MAX_ITEMS];

  int node_count;
  struct owl_model_node nodes[OWL_MODEL_MAX_ITEMS];

  int image_count;
  struct owl_model_image images[OWL_MODEL_MAX_ITEMS];

  int texture_count;
  struct owl_model_texture textures[OWL_MODEL_MAX_ITEMS];

  int material_count;
  struct owl_model_material materials[OWL_MODEL_MAX_ITEMS];

  int mesh_count;
  struct owl_model_mesh meshes[OWL_MODEL_MAX_ITEMS];

  int primitive_count;
  struct owl_model_primitive primitives[OWL_MODEL_MAX_ITEMS];

  int skin_count;
  struct owl_model_skin skins[OWL_MODEL_MAX_ITEMS];

  int anim_sampler_count;
  struct owl_model_anim_sampler anim_samplers[OWL_MODEL_MAX_ITEMS];

  int anim_chan_count;
  struct owl_model_anim_chan anim_chans[OWL_MODEL_MAX_ITEMS];

  int anim_count;
  struct owl_model_anim anims[OWL_MODEL_MAX_ITEMS];
};

OWL_PUBLIC owl_code
owl_model_init(struct owl_model *model, struct owl_renderer *renderer,
               char const *path);

OWL_PUBLIC void
owl_model_deinit(struct owl_model *model, struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_model_anim_update(struct owl_model *model, int frame, float dt,
                      owl_model_anim_id id);

OWL_END_DECLARATIONS

#endif

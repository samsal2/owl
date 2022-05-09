#ifndef OWL_MODEL_H_
#define OWL_MODEL_H_

#include "owl_renderer.h"
#include "owl_types.h"

#define OWL_MODEL_MESH_NONE -1
#define OWL_MODEL_ANIM_NONE -1
#define OWL_MODEL_SKIN_NONE -1
#define OWL_MODEL_NODE_PARENT_NONE -1
#define OWL_MODEL_TEXTURE_NONE -1
#define OWL_MODEL_MAX_PRIMITIVE_COUNT 128
#define OWL_MODEL_MESH_MAX_PRIMITIVE_COUNT 128
#define OWL_MODEL_MAX_NAME_LENGTH 128
#define OWL_MODEL_MAX_IMAGE_COUNT 16
#define OWL_MODEL_MAX_NODE_ROOT_COUNT 128
#define OWL_MODEL_MAX_NODE_COUNT 128
#define OWL_MODEL_MAX_TEXTURE_COUNT 8
#define OWL_MODEL_MAX_MATERIAL_COUNT 8
#define OWL_MODEL_NODE_MAX_CHILDREN_COUNT 128
#define OWL_MODEL_MAX_MESH_COUNT 128
#define OWL_MODEL_SKIN_MAX_JOINT_COUNT 128
#define OWL_MODEL_ANIM_SAMPLER_MAX_INPUT_COUNT 128
#define OWL_MODEL_ANIM_SAMPLER_MAX_OUTPUT_COUNT 128
#define OWL_MODEL_MAX_SKIN_COUNT 128
#define OWL_MODEL_MAX_SAMPLER_COUNT 128
#define OWL_MODEL_MAX_CHAN_COUNT 128
#define OWL_MODEL_ANIM_MAX_SAMPLER_COUNT 128
#define OWL_MODEL_ANIM_MAX_CHAN_COUNT 128
#define OWL_MODEL_MAX_ANIM_COUNT 128

struct owl_renderer;

typedef owl_i32 owl_model_node_descriptor;
typedef owl_i32 owl_model_mesh_descriptor;
typedef owl_i32 owl_model_texture_descriptor;
typedef owl_i32 owl_model_material_descriptor;
typedef owl_i32 owl_model_primitive_descriptor;
typedef owl_i32 owl_model_image_descriptor;
typedef owl_i32 owl_model_skin_descriptor;
typedef owl_i32 owl_model_anim_sampler_descriptor;
typedef owl_i32 owl_model_anim_chan_descriptor;
typedef owl_i32 owl_model_anim_descriptor;

/* NOTE(samuel): thought I knew how the alignment worked for the push constants,
 * turns out I don't. Change it at your own risk*/
struct owl_model_material_push_constant {
  owl_v4 base_color_factor;
  owl_v4 emissive_factor;
  owl_v4 diffuse_factor;
  owl_v4 specular_factor;
  float workflow;
  owl_i32 base_color_uv_set;
  owl_i32 physical_desc_uv_set;
  owl_i32 normal_uv_set;
  owl_i32 occlusion_uv_set;
  owl_i32 emissive_uv_set;
  float metallic_factor;
  float roughness_factor;
  float alpha_mask;
  float alpha_mask_cutoff;
};

struct owl_model1_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
  owl_v4 light;
};

struct owl_model2_ubo {
  owl_v4 light_direction;
};

struct owl_model_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_v4 joints0;
  owl_v4 weights0;
};

struct owl_model_primitive {
  owl_u32 first;
  owl_u32 count;
  owl_model_material_descriptor material;
};

struct owl_model_mesh {
  owl_i32 primitive_count;
  owl_model_primitive_descriptor primitives[OWL_MODEL_MESH_MAX_PRIMITIVE_COUNT];
};

struct owl_model_node {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;
  owl_m4 matrix;

  owl_model_node_descriptor parent;
  owl_model_mesh_descriptor mesh;
  owl_model_skin_descriptor skin;

  owl_i32 child_count;
  owl_model_node_descriptor children[OWL_MODEL_NODE_MAX_CHILDREN_COUNT];
};

struct owl_model_image {
  owl_renderer_image_descriptor renderer_image;
};

struct owl_model_texture {
  owl_model_image_descriptor model_image;
};

enum owl_alpha_mode {
  OWL_ALPHA_MODE_OPAQUE,
  OWL_ALPHA_MODE_MASK,
  OWL_ALPHA_MODE_BLEND
};

struct owl_model_material {
  enum owl_alpha_mode alpha_mode;
  float alpha_cutoff;
  owl_i32 double_sided;
  owl_model_texture_descriptor base_color_texture;
  owl_model_texture_descriptor normal_texture;
  owl_model_texture_descriptor physical_desc_texture;
  owl_model_texture_descriptor occlusion_texture;
  owl_model_texture_descriptor emissive_texture;
  owl_v4 base_color_factor;
};

struct owl_model_skin_ssbo {
  owl_m4 matrix;
  owl_m4 joint_matices[OWL_MODEL_SKIN_MAX_JOINT_COUNT];
  owl_i32 joint_matrice_count;
};

struct owl_model_skin {
  char name[OWL_MODEL_MAX_NAME_LENGTH];
  owl_model_node_descriptor skeleton_root;

  VkBuffer ssbo_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet ssbo_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

  owl_m4 inverse_bind_matrices[OWL_MODEL_SKIN_MAX_JOINT_COUNT];

  owl_i32 joint_count;
  owl_model_node_descriptor joints[OWL_MODEL_SKIN_MAX_JOINT_COUNT];

  VkDeviceMemory ssbo_memory;

  owl_u64 ssbo_buffer_size;
  owl_u64 ssbo_buffer_alignment;
  owl_u64 ssbo_buffer_aligned_size;

  /* NOTE(samuel): directly mapped skin joint data */
  struct owl_model_skin_ssbo *ssbo_datas[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
};

struct owl_model_anim_sampler {
  owl_i32 interpolation;

  owl_i32 input_count;
  float inputs[OWL_MODEL_ANIM_SAMPLER_MAX_INPUT_COUNT];

  owl_i32 output_count;
  owl_v4 outputs[OWL_MODEL_ANIM_SAMPLER_MAX_OUTPUT_COUNT];
};

struct owl_model_anim_chan {
  owl_i32 path;

  owl_model_node_descriptor node;
  owl_model_anim_sampler_descriptor anim_sampler;
};

struct owl_model_anim {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  float current_time;
  float begin;
  float end;

  owl_i32 sampler_count;
  owl_model_anim_sampler_descriptor samplers[OWL_MODEL_ANIM_MAX_SAMPLER_COUNT];

  owl_i32 chan_count;
  owl_model_anim_chan_descriptor chans[OWL_MODEL_ANIM_MAX_CHAN_COUNT];
};

struct owl_model {
  VkBuffer vertices_buffer;
  VkDeviceMemory vertices_memory;

  VkBuffer indices_buffer;
  VkDeviceMemory indices_memory;

  owl_model_anim_descriptor active_anim;

  owl_i32 root_count;
  owl_model_node_descriptor roots[OWL_MODEL_MAX_NODE_ROOT_COUNT];

  owl_i32 node_count;
  struct owl_model_node nodes[OWL_MODEL_MAX_NODE_COUNT];

  owl_i32 image_count;
  struct owl_model_image images[OWL_MODEL_MAX_IMAGE_COUNT];

  owl_i32 texture_count;
  struct owl_model_texture textures[OWL_MODEL_MAX_TEXTURE_COUNT];

  owl_i32 material_count;
  struct owl_model_material materials[OWL_MODEL_MAX_MATERIAL_COUNT];

  owl_i32 mesh_count;
  struct owl_model_mesh meshes[OWL_MODEL_MAX_MESH_COUNT];

  owl_i32 primitive_count;
  struct owl_model_primitive primitives[OWL_MODEL_MAX_PRIMITIVE_COUNT];

  owl_i32 skin_count;
  struct owl_model_skin skins[OWL_MODEL_MAX_SKIN_COUNT];

  owl_i32 anim_sampler_count;
  struct owl_model_anim_sampler anim_samplers[OWL_MODEL_MAX_SAMPLER_COUNT];

  owl_i32 anim_chan_count;
  struct owl_model_anim_chan anim_chans[OWL_MODEL_MAX_CHAN_COUNT];

  owl_i32 anim_count;
  struct owl_model_anim anims[OWL_MODEL_MAX_ANIM_COUNT];
};

enum owl_code owl_model_init(struct owl_model *model, struct owl_renderer *r,
                             char const *path);

void owl_model_deinit(struct owl_model *model, struct owl_renderer *r);

enum owl_code owl_model_anim_update(struct owl_model *model,
                                    owl_model_anim_descriptor animd,
                                    owl_i32 frame, float dt);

#endif

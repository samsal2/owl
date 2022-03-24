#ifndef OWL_MODEL_H_
#define OWL_MODEL_H_

#include "renderer.h"
#include "types.h"

#define OWL_MODEL_NODE_NO_MESH_SLOT -1
#define OWL_MODEL_NO_ANIMATION_SLOT -1
#define OWL_MODEL_NODE_NO_SKIN_SLOT -1
#define OWL_MODEL_NODE_NO_PARENT_SLOT -1
#define OWL_MODEL_MAX_PRIMITIVES_COUNT 128
#define OWL_MODEL_MESH_MAX_PRIMITIVES_COUNT 128
#define OWL_MODEL_MAX_NAME_LENGTH 128
#define OWL_MODEL_MAX_IMAGES_COUNT 16
#define OWL_MODEL_MAX_NODE_ROOTS_COUNT 128
#define OWL_MODEL_MAX_NODES_COUNT 128
#define OWL_MODEL_MAX_TEXTURES_COUNT 8
#define OWL_MODEL_MAX_MATERIALS_COUNT 8
#define OWL_MODEL_NODE_MAX_CHILDREN_COUNT 128
#define OWL_MODEL_MAX_MESHES_COUNT 128
#define OWL_MODEL_SKIN_MAX_INVERSE_BIND_MATRICES_COUNT 128
#define OWL_MODEL_SKIN_MAX_JOINTS_COUNT 128
#define OWL_MODEL_ANIMATION_SAMPLER_MAX_INPUTS_COUNT 128
#define OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT 128
#define OWL_MODEL_MAX_SKINS_COUNT 128
#define OWL_MODEL_MAX_SAMPLERS_COUNT 128
#define OWL_MODEL_MAX_CHANNELS_COUNT 128
#define OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT 128
#define OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT 128
#define OWL_MODEL_MAX_ANIMATIONS_COUNT 128

struct owl_renderer;

struct owl_model_node {
  owl_i32 slot;
};

struct owl_model_mesh {
  owl_i32 slot;
};

struct owl_model_texture {
  owl_i32 slot;
};

struct owl_model_material {
  owl_i32 slot;
};

struct owl_model_primitive {
  owl_i32 slot;
};

struct owl_model_image {
  owl_i32 slot;
};

struct owl_model_skin {
  owl_i32 slot;
};

struct owl_model_animation_sampler {
  owl_i32 slot;
};

struct owl_model_animation_channel {
  owl_i32 slot;
};

struct owl_model_animation {
  owl_i32 slot;
};

struct owl_model_push_constant {
  owl_m4 model;
};

struct owl_model_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_v4 light;
};

struct owl_model_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv;
  owl_v3 color;
  owl_v4 joints0;
  owl_v4 weights0;
};

struct owl_model_primitive_data {
  owl_u32 first;
  owl_u32 count;
  struct owl_model_material material;
};

struct owl_model_mesh_data {
  owl_i32 primitives_count;
  struct owl_model_primitive primitives[OWL_MODEL_MESH_MAX_PRIMITIVES_COUNT];
};

struct owl_model_node_data {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;
  owl_m4 matrix;

  struct owl_model_node parent;
  struct owl_model_mesh mesh;
  struct owl_model_skin skin;

  owl_i32 children_count;
  struct owl_model_node children[OWL_MODEL_NODE_MAX_CHILDREN_COUNT];
};

struct owl_model_image_data {
  struct owl_renderer_image image;
};

struct owl_model_texture_data {
  struct owl_model_image image;
};

enum owl_alpha_mode {
  OWL_ALPHA_MODE_OPAQUE,
  OWL_ALPHA_MODE_MASK,
  OWL_ALPHA_MODE_BLEND
};

struct owl_model_material_data {
  enum owl_alpha_mode alpha_mode;
  float alpha_cutoff;
  owl_i32 double_sided;
  struct owl_model_texture base_color_texture;
  struct owl_model_texture normal_texture;
  owl_v4 base_color_factor;
};

struct owl_model_skin_data {
  char name[OWL_MODEL_MAX_NAME_LENGTH];
  struct owl_model_node skeleton_root;

  VkDeviceMemory ssbo_memory;

  owl_u64 ssbo_buffer_size;
  owl_u64 ssbo_buffer_alignment;
  owl_u64 ssbo_buffer_aligned_size;

  owl_m4 *ssbo_datas[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkBuffer ssbo_buffers[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet ssbo_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];

  owl_i32 inverse_bind_matrices_count;
  owl_m4 inverse_bind_matrices[OWL_MODEL_SKIN_MAX_INVERSE_BIND_MATRICES_COUNT];

  owl_i32 joints_count;
  struct owl_model_node joints[OWL_MODEL_SKIN_MAX_JOINTS_COUNT];
};

struct owl_model_animation_sampler_data {
  owl_i32 interpolation;

  owl_i32 inputs_count;
  float inputs[OWL_MODEL_ANIMATION_SAMPLER_MAX_INPUTS_COUNT];

  owl_i32 outputs_count;
  owl_v4 outputs[OWL_MODEL_ANIMATION_SAMPLER_MAX_OUTPUTS_COUNT];
};

struct owl_model_animation_channel_data {
  owl_i32 path;

  struct owl_model_node node;
  struct owl_model_animation_sampler animation_sampler;
};

struct owl_model_animation_data {
  char name[OWL_MODEL_MAX_NAME_LENGTH];

  float current_time;
  float begin;
  float end;

  owl_i32 samplers_count;
  struct owl_model_animation_sampler
      samplers[OWL_MODEL_ANIMATION_MAX_SAMPLERS_COUNT];

  owl_i32 channels_count;
  struct owl_model_animation_channel
      channels[OWL_MODEL_ANIMATION_MAX_CHANNELS_COUNT];
};

struct owl_model {
  VkBuffer vertices_buffer;
  VkDeviceMemory vertices_memory;

  VkBuffer indices_buffer;
  VkDeviceMemory indices_memory;

  struct owl_model_animation active_animation;

  owl_i32 roots_count;
  struct owl_model_node roots[OWL_MODEL_MAX_NODE_ROOTS_COUNT];

  owl_i32 nodes_count;
  struct owl_model_node_data nodes[OWL_MODEL_MAX_NODES_COUNT];

  owl_i32 images_count;
  struct owl_model_image_data images[OWL_MODEL_MAX_IMAGES_COUNT];

  owl_i32 textures_count;
  struct owl_model_texture_data textures[OWL_MODEL_MAX_TEXTURES_COUNT];

  owl_i32 materials_count;
  struct owl_model_material_data materials[OWL_MODEL_MAX_MATERIALS_COUNT];

  owl_i32 meshes_count;
  struct owl_model_mesh_data meshes[OWL_MODEL_MAX_MESHES_COUNT];

  owl_i32 primitives_count;
  struct owl_model_primitive_data primitives[OWL_MODEL_MAX_PRIMITIVES_COUNT];

  owl_i32 skins_count;
  struct owl_model_skin_data skins[OWL_MODEL_MAX_SKINS_COUNT];

  owl_i32 animation_samplers_count;
  struct owl_model_animation_sampler_data
      animation_samplers[OWL_MODEL_MAX_SAMPLERS_COUNT];

  owl_i32 animation_channels_count;
  struct owl_model_animation_channel_data
      animation_channels[OWL_MODEL_MAX_CHANNELS_COUNT];

  owl_i32 animations_count;
  struct owl_model_animation_data animations[OWL_MODEL_MAX_ANIMATIONS_COUNT];
};

enum owl_code owl_model_init(struct owl_renderer *renderer, char const *path,
                             struct owl_model *model);

void owl_model_deinit(struct owl_renderer *renderer, struct owl_model *model);

void owl_model_update_animation(struct owl_renderer const *renderer,
                                struct owl_model *model, float dt);

#endif

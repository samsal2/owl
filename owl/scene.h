#ifndef OWL_SCENE_H_
#define OWL_SCENE_H_

#include "image.h"
#include "types.h"

#define OWL_SCENE_NODE_NO_PARENT_SLOT -1
#define OWL_SCENE_MAX_PRIMITIVES_SIZE 256
#define OWL_SCENE_MESH_MAX_PRIMITIVES_SIZE 128
#define OWL_SCENE_MAX_NAME_SIZE 128
#define OWL_SCENE_MAX_IMAGES_SIZE 16
#define OWL_SCENE_MAX_NODE_ROOTS_SIZE 32
#define OWL_SCENE_MAX_NODES_SIZE 32
#define OWL_SCENE_MAX_TEXTURES_SIZE 8
#define OWL_SCENE_MAX_MATERIALS_SIZE 8
#define OWL_SCENE_NODE_MAX_CHILDREN_SIZE 32
#define OWL_SCENE_MAX_MESHES_SIZE 32
#define OWL_SCENE_SKIN_MAX_INVERSE_BIND_MATRICES_SIZE 32
#define OWL_SCENE_SKIN_MAX_JOINTS_SIZE 8
#define OWL_SCENE_ANIMATION_SAMPLER_MAX_INPUTS_SIZE 32
#define OWL_SCENE_ANIMATION_SAMPLER_MAX_OUTPUTS_SIZE 32
#define OWL_SCENE_MAX_SKINS_SIZE 32
#define OWL_SCENE_MAX_SAMPLERS_SIZE 32
#define OWL_SCENE_MAX_CHANNELS_SIZE 32
#define OWL_SCENE_ANIMATION_MAX_SAMPLERS_SIZE 32
#define OWL_SCENE_ANIMATION_MAX_CHANNELS_SIZE 32
#define OWL_SCENE_MAX_ANIMATIONS_SIZE 32

struct owl_renderer;

struct owl_scene_node {
  int slot;
};

struct owl_scene_mesh {
  int slot;
};

struct owl_scene_texture {
  int slot;
};

struct owl_scene_material {
  int slot;
};

struct owl_scene_primitive {
  int slot;
};

struct owl_scene_image {
  int slot;
};

struct owl_scene_skin {
  int slot;
};

struct owl_scene_animation_sampler {
  int slot;
};

struct owl_scene_animation_channel {
  int slot;
};

struct owl_scene_push_constant {
  owl_m4 model;
};

struct owl_scene_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_v4 light;
  owl_v4 eye;
  int enable_alpha_mask;
  float alpha_cutoff;
};

struct owl_scene_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv;
  owl_v3 color;
  owl_v4 tangent;
};

struct owl_scene_primitive_data {
  owl_u32 first;
  owl_u32 size;
  struct owl_scene_material material;
};

struct owl_scene_mesh_data {
  int primitives_size;
  struct owl_scene_primitive primitives[OWL_SCENE_MESH_MAX_PRIMITIVES_SIZE];
};

struct owl_scene_node_data {
  char name[OWL_SCENE_MAX_NAME_SIZE];

  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;
  owl_m4 matrix;

  struct owl_scene_node parent;
  struct owl_scene_mesh mesh;

  int children_size;
  struct owl_scene_node children[OWL_SCENE_NODE_MAX_CHILDREN_SIZE];
};

struct owl_scene_image_data {
  struct owl_image image;
};

struct owl_scene_texture_data {
  struct owl_scene_image image;
};

enum owl_alpha_mode {
  OWL_ALPHA_MODE_OPAQUE,
  OWL_ALPHA_MODE_MASK,
  OWL_ALPHA_MODE_BLEND
};

struct owl_scene_material_data {
  enum owl_alpha_mode alpha_mode;
  float alpha_cutoff;
  int double_sided;
  struct owl_scene_texture base_color_texture;
  struct owl_scene_texture normal_texture;
  owl_v4 base_color_factor;
};

struct owl_scene_skin_data {
  char name[OWL_SCENE_MAX_NAME_SIZE];
  struct owl_scene_node skeleton_root;

  VkBuffer ssbo_buffer;
  VkDeviceMemory ssbo_memory;
  VkDescriptorSet ssbo_set;

  int inverse_bind_matrices_size;
  owl_m4 inverse_bind_matrices[OWL_SCENE_SKIN_MAX_INVERSE_BIND_MATRICES_SIZE];

  int joints_size;
  struct owl_scene_node joints[OWL_SCENE_SKIN_MAX_JOINTS_SIZE];
};

struct owl_scene_animation_sampler_data {
  int interpolation;

  int inputs_size;
  float inputs[OWL_SCENE_ANIMATION_SAMPLER_MAX_INPUTS_SIZE];

  int outputs_size;
  owl_v4 outputs[OWL_SCENE_ANIMATION_SAMPLER_MAX_OUTPUTS_SIZE];
};

struct owl_scene_animation_channel_data {
  int path;

  struct owl_scene_node node;
  struct owl_scene_animation_sampler animation_sampler;
};

struct owl_scene_animation_data {
  char name[OWL_SCENE_MAX_NAME_SIZE];

  float begin;
  float end;

  int samplers_size;
  struct owl_scene_animation_sampler
      samplers[OWL_SCENE_ANIMATION_MAX_SAMPLERS_SIZE];

  int channels_size;
  struct owl_scene_animation_channel
      channels[OWL_SCENE_ANIMATION_MAX_CHANNELS_SIZE];
};

struct owl_scene {
  VkBuffer vertices_buffer;
  VkDeviceMemory vertices_memory;

  VkBuffer indices_buffer;
  VkDeviceMemory indices_memory;

  int roots_size;
  struct owl_scene_node roots[OWL_SCENE_MAX_NODE_ROOTS_SIZE];

  int nodes_size;
  struct owl_scene_node_data nodes[OWL_SCENE_MAX_NODES_SIZE];

  int images_size;
  struct owl_scene_image_data images[OWL_SCENE_MAX_IMAGES_SIZE];

  int textures_size;
  struct owl_scene_texture_data textures[OWL_SCENE_MAX_TEXTURES_SIZE];

  int materials_size;
  struct owl_scene_material_data materials[OWL_SCENE_MAX_MATERIALS_SIZE];

  int meshes_size;
  struct owl_scene_mesh_data meshes[OWL_SCENE_MAX_MESHES_SIZE];

  int primitives_size;
  struct owl_scene_primitive_data primitives[OWL_SCENE_MAX_PRIMITIVES_SIZE];

  int skins_size;
  struct owl_scene_skin_data skins[OWL_SCENE_MAX_SKINS_SIZE];

  int animation_samplers_size;
  struct owl_scene_animation_sampler_data
      animation_samplers[OWL_SCENE_MAX_SAMPLERS_SIZE];

  int animation_channels_size;
  struct owl_scene_animation_channel_data
      animation_channels[OWL_SCENE_MAX_CHANNELS_SIZE];

  int animations_size;
  struct owl_scene_animation_data animations[OWL_SCENE_MAX_ANIMATIONS_SIZE];
};

enum owl_code owl_scene_init(struct owl_renderer *r, char const *path,
                             struct owl_scene *scene);

void owl_scene_deinit(struct owl_renderer *r, struct owl_scene *scene);

#endif

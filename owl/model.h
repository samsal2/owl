#ifndef OWL_MODEL_H_
#define OWL_MODEL_H_

#include "draw.h"
#include "texture.h"

#include <vulkan/vulkan.h>

#define OWL_MODEL_MAX_NODES 32
#define OWL_MODEL_MAX_TEXTURES 32
#define OWL_MODEL_MAX_MATERIALS 32
#define OWL_MODEL_MAX_MESHES 32
#define OWL_MODEL_MAX_PRIMITIVES_PER_MESH 32
#define OWL_MODEL_MAX_CHILDREN_PER_NODE 32

struct owl_material_push_constant {
  owl_v4 base_color_factor;
  owl_v4 emissive_factor;
  owl_v4 diffuse_factor;
  owl_v4 specular_factor;
  float workflow;
  int base_color_texture_set;
  int physical_descriptor_texture_set;
  int normal_texture_set;
  int occlusion_texture_set;
  int emissive_texture_set;
  float metallic_factor;
  float roughness_factor;
  float alpha_mask;
  float alpha_mask_cutoff;
};

struct owl_model_node {
  int handle;
};

struct owl_model_texture {
  int handle;
};

struct owl_model_material {
  int handle;
};

struct owl_model_mesh {
  int handle;
};

struct owl_model_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_v4 joint0;
  owl_v4 weight0;
};

struct owl_model_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
  owl_v3 camera_position;
};

struct owl_model_texture_data {
  char uri[256];

  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  owl_u32 layer_count;

  VkDeviceMemory memory;
  VkImage image;
  VkImageLayout image_layout;
  VkImageView view;
  VkSampler sampler;
};

enum owl_model_material_alpha_mode {
  OWL_MODEL_MATERIAL_ALPHA_MODE_OPAQUE,
  OWL_MODEL_MATERIAL_ALPHA_MODE_MASK,
  OWL_MODEL_MATERIAL_ALPHA_MODE_BLEND
};

struct owl_model_material_data {
  enum owl_model_material_alpha_mode alpha_mode;
  char const *name;

  float alpha_cutoff;
  float metallic_factor;
  float roughness_factor;

  int base_color_texcoord;
  int metallic_roughness_texcoord;
  int specular_glossines_texcoord;
  int normal_texcoord;
  int emissive_texcoord;
  int occlusion_texcoord;

  struct owl_model_texture base_color_texture;
  struct owl_model_texture metallic_roughness_texture;
  struct owl_model_texture normal_texture;
  struct owl_model_texture occlusion_texture;
  struct owl_model_texture emissive_texture;

  owl_v4 base_color_factor;
  owl_v4 emissive_factor;

  VkDescriptorSet set;
};

struct owl_model_primitive {
  int has_indices;

  owl_u32 first_index;
  owl_u32 indices_count;
  owl_u32 vertex_count;

  struct owl_model_material material;
};

#define OWL_MODEL_MAX_JOINTS 8

struct owl_model_mesh_uniform {
  int joint_count;
  owl_m4 matrix;
  owl_m4 joints[OWL_MODEL_MAX_JOINTS];
};

struct owl_model_mesh_data {
  struct owl_model_mesh_uniform uniform;

  void *data;
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorSet set;

  int primitives_count;
  struct owl_model_primitive primitives[OWL_MODEL_MAX_PRIMITIVES_PER_MESH];
};

struct owl_model_node_data {
  struct owl_model_node parent;

  struct owl_model_mesh mesh;

  int children_count;
  struct owl_model_node children[OWL_MODEL_MAX_CHILDREN_PER_NODE];
};

struct owl_model {
  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_memory;

  VkBuffer index_buffer;
  VkDeviceMemory index_memory;

  int roots_count;
  struct owl_model_node roots[OWL_MODEL_MAX_NODES];

  int nodes_count;
  struct owl_model_node_data nodes[OWL_MODEL_MAX_NODES];

  int textures_count;
  struct owl_model_texture_data textures[OWL_MODEL_MAX_TEXTURES];

  int materials_count;
  struct owl_model_material_data materials[OWL_MODEL_MAX_MATERIALS];

  int meshes_count;
  struct owl_model_mesh_data meshes[OWL_MODEL_MAX_MESHES];
};

enum owl_code owl_model_init_from_file(struct owl_renderer *r, char const *path,
                                       struct owl_model *model);

void owl_model_deinit(struct owl_renderer *r, struct owl_model *model);

enum owl_code owl_model_submit(struct owl_renderer *r,
                               struct owl_model const *model);

#endif

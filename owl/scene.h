#ifndef OWL_SCENE_H_
#define OWL_SCENE_H_

#include "image.h"
#include "types.h"

#define OWL_SCENE_NODE_NO_PARENT -1
#define OWL_SCENE_MESH_MAX_PRIMITIVES_COUNT 128
#define OWL_SCENE_NODE_MAX_NAME_LENGTH 128
#define OWL_SCENE_MAX_IMAGES_COUNT 16
#define OWL_SCENE_MAX_NODE_ROOTS_COUNT 32
#define OWL_SCENE_MAX_NODES_COUNT 32
#define OWL_SCENE_MAX_TEXTURES_COUNT 8
#define OWL_SCENE_MAX_MATERIALS_COUNT 8
#define OWL_SCENE_NODE_MAX_CHILDREN_COUNT 32
#define OWL_SCENE_MAX_MESHES_COUNT 32

struct owl_renderer;

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

struct owl_scene_primitive {
  owl_u32 first;
  owl_u32 count;
  int material;
};

struct owl_scene_mesh_data {
  int primitives_count;
  struct owl_scene_primitive primitives[OWL_SCENE_MESH_MAX_PRIMITIVES_COUNT];
};

typedef int owl_scene_node;
typedef int owl_scene_mesh;

struct owl_scene_node_data {
  char name[OWL_SCENE_NODE_MAX_NAME_LENGTH];

  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;
  owl_m4 matrix;

  owl_scene_node parent;
  owl_scene_mesh mesh;

  int children_count;
  owl_scene_node children[OWL_SCENE_NODE_MAX_CHILDREN_COUNT];
};

enum owl_alpha_mode {
  OWL_ALPHA_MODE_OPAQUE,
  OWL_ALPHA_MODE_MASK,
  OWL_ALPHA_MODE_BLEND
};

typedef int owl_scene_texture;

struct owl_scene_material {
  enum owl_alpha_mode alpha_mode;
  float alpha_cutoff;
  int double_sided;
  owl_scene_texture base_color_texture;
  owl_scene_texture normal_texture;
  owl_v4 base_color_factor;
};

struct owl_scene {
  VkBuffer vertices_buffer;
  VkDeviceMemory vertices_memory;

  VkBuffer indices_buffer;
  VkDeviceMemory indices_memory;

  int roots_count;
  owl_scene_node roots[OWL_SCENE_MAX_NODE_ROOTS_COUNT];

  int nodes_count;
  struct owl_scene_node_data nodes[OWL_SCENE_MAX_NODES_COUNT];

  int images_count;
  struct owl_image images[OWL_SCENE_MAX_IMAGES_COUNT];

  int textures_count;
  owl_scene_texture textures[OWL_SCENE_MAX_TEXTURES_COUNT];

  int materials_count;
  struct owl_scene_material materials[OWL_SCENE_MAX_MATERIALS_COUNT];

  int meshes_count;
  struct owl_scene_mesh_data meshes[OWL_SCENE_MAX_MESHES_COUNT];
};

enum owl_code owl_scene_init(struct owl_renderer *r, char const *path,
                             struct owl_scene *scene);

void owl_scene_deinit(struct owl_renderer *r, struct owl_scene *scene);

#endif

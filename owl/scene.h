#ifndef OWL_SCENE_H_
#define OWL_SCENE_H_

#include "texture.h"
#include "types.h"

#define OWL_SCENE_NODE_NO_PARENT -1
#define OWL_SCENE_MESH_MAX_PRIMITIVES 128
#define OWL_SCENE_NODE_MAX_NAME_LENGTH 128
#define OWL_SCENE_MAX_IMAGES 8
#define OWL_SCENE_MAX_NODE_ROOTS 128
#define OWL_SCENE_MAX_NODES 128
#define OWL_SCENE_MAX_TEXTURES 16
#define OWL_SCENE_MAX_MATERIALS 8
#define OWL_SCENE_NODE_MAX_CHILDREN 32

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

struct owl_scene_mesh {
  int primitives_count;
  struct owl_scene_primitive primitives[OWL_SCENE_MESH_MAX_PRIMITIVES];
};

typedef int owl_scene_node;

struct owl_scene_node_data {
  char name[OWL_SCENE_NODE_MAX_NAME_LENGTH];
  owl_scene_node parent;

  owl_m4 matrix;
  owl_v3 translation;
  owl_v3 scale;
  owl_v4 rotation;

  int skin;

  struct owl_scene_mesh mesh;

  int children_count;
  owl_scene_node children[OWL_SCENE_NODE_MAX_CHILDREN];
};

enum owl_alpha_mode {
  OWL_ALPHA_MODE_OPAQUE,
  OWL_ALPHA_MODE_MASK,
  OWL_ALPHA_MODE_BLEND
};

struct owl_scene_material {
  int double_sided;
  float alpha_cutoff;
  owl_v4 base_color_factor;
  int base_color_texture_index;
  int normal_texture_index;
  enum owl_alpha_mode alpha_mode;
};

typedef int owl_scene_texture;

struct owl_scene {
  VkBuffer vertices_buffer;
  VkDeviceMemory vertices_memory;

  VkBuffer indices_buffer;
  VkDeviceMemory indices_memory;

  int roots_count;
  owl_scene_node roots[OWL_SCENE_MAX_NODE_ROOTS];

  int nodes_count;
  struct owl_scene_node_data nodes[OWL_SCENE_MAX_NODES];

  int images_count;
  struct owl_texture images[OWL_SCENE_MAX_IMAGES];

  int textures_count;
  owl_scene_texture textures[OWL_SCENE_MAX_TEXTURES];

  int materials_count;
  struct owl_scene_material materials[OWL_SCENE_MAX_MATERIALS];
};

enum owl_code owl_scene_init(struct owl_renderer *r, char const *path,
                             struct owl_scene *scene);

void owl_scene_deinit(struct owl_renderer *r, struct owl_scene *scene);

#endif

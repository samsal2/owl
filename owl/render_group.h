#ifndef OWL_RENDER_GROUP_H_
#define OWL_RENDER_GROUP_H_

#include <owl/code.h>
#include <owl/fwd.h>
#include <owl/types.h>

struct owl_vertex {
  OwlV3 pos;
  OwlV3 color;
  OwlV2 uv;
};

struct owl_pvm {
  OwlM4 proj;
  OwlM4 view;
  OwlM4 model;
};

struct owl_lighting {
  OwlV3 light;
  OwlV3 view;
};

enum owl_render_group_type {
  OWL_RENDER_GROUP_TYPE_NOP,
  OWL_RENDER_GROUP_TYPE_BASIC,
  OWL_RENDER_GROUP_TYPE_QUAD,
  OWL_RENDER_GROUP_LIGHT_TEST
};

struct owl_render_group_basic {
  OwlTexture texture;
  OwlU32 vertex_count;
  OwlU32 index_count;
  OwlU32 const *index;
  struct owl_vertex const *vertex;
  struct owl_pvm pvm;
};

struct owl_render_group_quad {
  OwlTexture texture;
  struct owl_vertex vertex[4];
  struct owl_pvm pvm;
};

struct owl_render_group_light_test {
  OwlTexture texture;
  struct owl_vertex vertex[4];
  struct owl_pvm pvm;
  struct owl_lighting lighting;
};

struct owl_render_group_storage {
  struct owl_render_group_basic as_basic;
  struct owl_render_group_quad as_quad;
  struct owl_render_group_light_test as_light;
};

struct owl_render_group {
  enum owl_render_group_type type;
  struct owl_render_group_storage storage;
};

enum owl_code owl_submit_render_group(struct owl_vk_renderer *renderer,
                                      struct owl_render_group const *group);

#endif

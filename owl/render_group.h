#ifndef OWL_RENDER_GROUP_H_
#define OWL_RENDER_GROUP_H_

#include <owl/fwd.h>
#include <owl/code.h>
#include <owl/types.h>

struct owl_vertex {
  OwlV3 pos;
  OwlV3 color;
  OwlV2 uv;
};

struct owl_uniform {
  OwlM4 proj;
  OwlM4 view;
  OwlM4 model;
};

enum owl_render_group_type {
  OWL_RENDER_GROUP_TYPE_NOP,
  OWL_RENDER_GROUP_TYPE_BASIC,
  OWL_RENDER_GROUP_TYPE_QUAD
};

struct owl_render_group_basic {
  OwlTexture texture;
  OwlU32 vertex_count;
  OwlU32 index_count;
  OwlU32 const *index;
  struct owl_vertex const *vertex;
  struct owl_uniform pvm;
};

struct owl_render_group_quad {
  OwlTexture texture;
  struct owl_vertex vertex[4];
  struct owl_uniform pvm;
};

struct owl_render_group_storage {
  struct owl_render_group_basic as_basic;
  struct owl_render_group_quad as_quad;
};

struct owl_render_group {
  enum owl_render_group_type type;
  struct owl_render_group_storage storage;
};

enum owl_code owl_submit_render_group(struct owl_renderer *renderer,
                                      struct owl_render_group const *group);

#endif

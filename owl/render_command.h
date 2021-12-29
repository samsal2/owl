#ifndef OWL_RENDER_COMMAND_H_
#define OWL_RENDER_COMMAND_H_

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

enum owl_render_command_type {
  OWL_RENDER_COMMAND_TYPE_NOP,
  OWL_RENDER_COMMAND_TYPE_BASIC,
  OWL_RENDER_COMMAND_TYPE_QUAD
};

struct owl_render_command_basic {
  OwlTexture texture;
  OwlU32 vertex_count;
  OwlU32 index_count;
  OwlU32 const *index;
  struct owl_vertex const *vertex;
  struct owl_pvm pvm;
};

struct owl_render_command_quad {
  OwlTexture texture;
  struct owl_vertex vertex[4];
  struct owl_pvm pvm;
};

struct owl_render_command_storage {
  struct owl_render_command_basic as_basic;
  struct owl_render_command_quad as_quad;
};

struct owl_render_command {
  enum owl_render_command_type type;
  struct owl_render_command_storage storage;
};

enum owl_code owl_submit_render_group(struct owl_vk_renderer *renderer,
                                      struct owl_render_command const *group);

#endif

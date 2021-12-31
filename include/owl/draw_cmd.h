#ifndef OWL_INCLUDE_DRAW_CMD_H_
#define OWL_INCLUDE_DRAW_CMD_H_

#include "code.h"
#include "types.h"

struct owl_img;
struct owl_vk_renderer;

struct owl_draw_cmd_vertex {
  OwlV3 pos;
  OwlV3 color;
  OwlV2 uv;
};

struct owl_draw_cmd_ubo {
  OwlM4 proj;
  OwlM4 view;
  OwlM4 model;
};

enum owl_draw_cmd_type { OWL_DRAW_CMD_TYPE_BASIC, OWL_DRAW_CMD_TYPE_QUAD };

struct owl_draw_cmd_basic {
  struct owl_img const *img;
  struct owl_draw_cmd_ubo ubo;

  OwlU32 index_count;
  OwlU32 const *indices;

  OwlU32 vertex_count;
  struct owl_draw_cmd_vertex const *vertices;
};

struct owl_draw_cmd_quad {
  struct owl_img const *img;
  struct owl_draw_cmd_ubo ubo;
  struct owl_draw_cmd_vertex vertices[4];
};

union owl_draw_cmd_storage {
  struct owl_draw_cmd_basic as_basic;
  struct owl_draw_cmd_quad as_quad;
};

struct owl_draw_cmd {
  enum owl_draw_cmd_type type;
  union owl_draw_cmd_storage storage;
};

enum owl_code owl_submit_draw_cmd(struct owl_vk_renderer *renderer,
                                  struct owl_draw_cmd const *cmd);

#endif

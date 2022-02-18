#ifndef OWL_DRAW_CMD_H_
#define OWL_DRAW_CMD_H_

#include "types.h"

struct owl_vk_renderer;

struct owl_draw_cmd_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_draw_cmd_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

enum owl_draw_cmd_type {
  OWL_DRAW_CMD_TYPE_BASIC, /**/
  OWL_DRAW_CMD_TYPE_QUAD,
  OWL_DRAW_CMD_TYPE_SKYBOX,
  OWL_DRAW_CMD_TYPE_TEXT
};

struct owl_draw_cmd_basic {
  struct owl_texture const *texture;
  struct owl_draw_cmd_ubo ubo;

  owl_u32 indices_count;
  owl_u32 const *indices;

  owl_u32 vertices_count;
  struct owl_draw_cmd_vertex const *vertices;
};

struct owl_draw_cmd_quad {
  struct owl_texture const *texture;
  struct owl_draw_cmd_ubo ubo;
  struct owl_draw_cmd_vertex vertices[4];
};

struct owl_draw_cmd_skybox {
  struct owl_draw_cmd_ubo ubo;
  struct owl_skybox const *skybox;
};

struct owl_draw_cmd_text {
  owl_v3 color;
  owl_v2 position;
  char const *text;
  struct owl_font const *font;
};

union owl_draw_cmd_storage {
  struct owl_draw_cmd_basic as_basic;
  struct owl_draw_cmd_quad as_quad;
  struct owl_draw_cmd_skybox as_skybox;
  struct owl_draw_cmd_text as_text;
};

struct owl_draw_cmd {
  enum owl_draw_cmd_type type;
  union owl_draw_cmd_storage storage;
};

enum owl_code owl_draw_cmd_submit(struct owl_vk_renderer *r,
                                  struct owl_draw_cmd const *cmd);

#endif

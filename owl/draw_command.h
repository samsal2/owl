#ifndef OWL_DRAW_COMMAND_H_
#define OWL_DRAW_COMMAND_H_

#include "types.h"

struct owl_renderer;

struct owl_draw_command_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_draw_command_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

enum owl_draw_command_type {
  OWL_DRAW_COMMAND_TYPE_BASIC, /**/
  OWL_DRAW_COMMAND_TYPE_QUAD,
  OWL_DRAW_COMMAND_TYPE_SKYBOX,
  OWL_DRAW_COMMAND_TYPE_TEXT
};

struct owl_draw_command_basic {
  struct owl_texture const *texture;
  struct owl_draw_command_ubo ubo;

  owl_u32 indices_count;
  owl_u32 const *indices;

  owl_u32 vertices_count;
  struct owl_draw_command_vertex const *vertices;
};

struct owl_draw_command_quad {
  struct owl_texture const *texture;
  struct owl_draw_command_ubo ubo;
  struct owl_draw_command_vertex vertices[4];
};

struct owl_draw_command_skybox {
  struct owl_draw_command_ubo ubo;
  struct owl_skybox const *skybox;
};

struct owl_draw_command_text {
  owl_v3 color;
  owl_v2 position;
  char const *text;
  struct owl_font const *font;
};

union owl_draw_command_storage {
  struct owl_draw_command_basic as_basic;
  struct owl_draw_command_quad as_quad;
  struct owl_draw_command_skybox as_skybox;
  struct owl_draw_command_text as_text;
};

struct owl_draw_command {
  enum owl_draw_command_type type;
  union owl_draw_command_storage storage;
};

enum owl_code owl_draw_command_submit(struct owl_renderer *r,
                                      struct owl_draw_command const *command);

#endif

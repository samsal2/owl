#ifndef OWL_DRAW_H_
#define OWL_DRAW_H_

#include "renderer.h"
#include "types.h"

struct owl_renderer;
struct owl_model;
struct owl_camera;

struct owl_draw_command_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_draw_command_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

struct owl_draw_command_basic {
  struct owl_renderer_image image;
  owl_m4 model;

  owl_u32 indices_count;
  owl_u32 const *indices;

  owl_u32 vertices_count;
  struct owl_draw_command_vertex const *vertices;
};

enum owl_code
owl_draw_command_submit_basic(struct owl_renderer *r,
                              struct owl_camera const *cam,
                              struct owl_draw_command_basic const *command);

struct owl_draw_command_quad {
  struct owl_renderer_image image;
  owl_m4 model;
  struct owl_draw_command_vertex vertices[4];
};

enum owl_code
owl_draw_command_submit_quad(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_command_quad const *command);

struct owl_draw_command_text {
  float scale;
  owl_v3 color;
  owl_v3 position;
  char const *text;
  struct owl_font const *font;
};

enum owl_code
owl_draw_command_submit_text(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_command_text const *command);

struct owl_draw_command_model {
  owl_v3 light;
  owl_m4 model;
  struct owl_model const *skin;
};

enum owl_code
owl_draw_command_submit_model(struct owl_renderer *r,
                              struct owl_camera const *c,
                              struct owl_draw_command_model const *command);

struct owl_draw_command_grid {
  struct owl_infinite_grid const *grid;
};

enum owl_code
owl_draw_command_submit_grid(struct owl_renderer *r, struct owl_camera const *c,
                             struct owl_draw_command_grid const *command);

#endif

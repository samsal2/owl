#ifndef OWL_DRAW_H_
#define OWL_DRAW_H_

#include "owl_renderer.h"
#include "owl_types.h"

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
owl_draw_command_basic_submit(struct owl_draw_command_basic const *cmd,
                              struct owl_renderer *r,
                              struct owl_camera const *cam);

struct owl_draw_command_quad {
  struct owl_renderer_image image;
  owl_m4 model;
  /*
   * 0 - 1
   * | / |
   * 2 - 3
   */
  struct owl_draw_command_vertex vertices[4];
};

enum owl_code
owl_draw_command_quad_submit(struct owl_draw_command_quad const *cmd,
                             struct owl_renderer *r,
                             struct owl_camera const *cam);

struct owl_draw_command_text {
  float scale; /* FIXME(samuel): unused for now */
  owl_v3 color;
  owl_v3 position;
  char const *text;
  struct owl_font const *font;
};

enum owl_code
owl_draw_command_text_submit(struct owl_draw_command_text const *cmd,
                             struct owl_renderer *r,
                             struct owl_camera const *cam);

struct owl_draw_command_model {
  owl_v3 light;
  owl_m4 model;
  struct owl_model const *skin;
};

enum owl_code
owl_draw_command_model_submit(struct owl_draw_command_model const *cmd,
                              struct owl_renderer *r,
                              struct owl_camera const *cam);

#endif

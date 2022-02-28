#ifndef OWL_DRAW_H_
#define OWL_DRAW_H_

#include "types.h"

struct owl_renderer;
struct owl_skybox;
struct owl_scene;
struct owl_camera;

struct owl_draw_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_draw_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

struct owl_draw_basic_command {
  struct owl_texture const *texture;
  owl_m4 model;

  owl_u32 indices_count;
  owl_u32 const *indices;

  owl_u32 vertices_count;
  struct owl_draw_vertex const *vertices;
};

enum owl_code
owl_submit_draw_basic_command(struct owl_renderer *r,
                              struct owl_camera const *cam,
                              struct owl_draw_basic_command const *command);

struct owl_draw_quad_command {
  struct owl_texture const *texture;
  owl_m4 model;
  struct owl_draw_vertex vertices[4];
};

enum owl_code
owl_submit_draw_quad_command(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_quad_command const *command);

struct owl_draw_text_command {
  owl_v3 color;
  owl_v3 position;
  char const *text;
  struct owl_font const *font;
};

enum owl_code
owl_submit_draw_text_command(struct owl_renderer *r,
                             struct owl_camera const *cam,
                             struct owl_draw_text_command const *command);

struct owl_draw_skybox_command {
  struct owl_skybox const *skybox;
};

enum owl_code
owl_submit_draw_skybox_command(struct owl_renderer *r,
                               struct owl_camera const *cam,
                               struct owl_draw_skybox_command const *command);

struct owl_draw_scene_command {
  owl_v3 light;
  struct owl_scene const *scene;
};

enum owl_code
owl_submit_draw_scene_command(struct owl_renderer *r, struct owl_camera *cam,
                              struct owl_draw_scene_command const *command);

#endif

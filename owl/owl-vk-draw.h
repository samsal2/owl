#ifndef OWL_VK_DRAW_H_
#define OWL_VK_DRAW_H_

#include "owl-definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;
struct owl_model;
struct owl_vk_texture;

struct owl_quad {
  owl_v2 position0;
  owl_v2 position1;
  owl_v3 color;
  owl_v2 uv0;
  owl_v2 uv1;
  struct owl_vk_texture *texture;
};

owl_public owl_code
owl_vk_draw_quad(struct owl_vk_renderer *vk, struct owl_quad const *quad,
                 owl_m4 const matrix);

owl_public owl_code
owl_vk_draw_skybox(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_draw_text(struct owl_vk_renderer *vk, char const *text,
                 owl_v3 const position, owl_v3 const color);

owl_public owl_code
owl_vk_draw_model(struct owl_vk_renderer *vk, struct owl_model const *model,
                  owl_m4 const matrix);

OWL_END_DECLS

#endif

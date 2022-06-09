#ifndef OWL_VK_DRAW_H_
#define OWL_VK_DRAW_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;
struct owl_model;
struct owl_vk_texture;

struct owl_quad {
  /*
   * 0 - x
   * |   |
   * x - 1
   */
  /** normalized top left corner position */
  owl_v2 position0;
  /** normalized bottom right corner position */
  owl_v2 position1;
  /** color blend value */
  owl_v3 color;
  /** normalized top left uv */
  owl_v2 uv0;
  /** normalized bottom right uv  */
  owl_v2 uv1;
  /** texture to draw */
  struct owl_vk_texture *texture;
};

/**
 * @brief draws a quad to the specifications of the quad struct
 *
 * @param vk the renderer instance created with owl_vk_renderer_init(...)
 * @param quad the quad struct describing a quad
 * @param matrix the model matrix
 * @return owl_code
 */
owl_public owl_code
owl_vk_draw_quad(struct owl_vk_renderer *vk, struct owl_quad const *quad,
                 owl_m4 const matrix);

/**
 * @brief draw the loaded skybox
 *
 * @param vk the renderer instance created with owl_vk_renderer_init(...)
 * @return owl_code
 */
owl_public owl_code
owl_vk_draw_skybox(struct owl_vk_renderer *vk);

/**
 * @brief draws text using the loaded font
 *
 * @param vk the renderer instance created with owl_vk_renderer_init(...)
 * @param text the text to draw
 * @param position the position of the text in normalized coordinates
 * @param color  the color of the text in normalized values
 * @return owl_code
 */
owl_public owl_code
owl_vk_draw_text(struct owl_vk_renderer *vk, char const *text,
                 owl_v3 const position, owl_v3 const color);

/**
 * @brief draw a model
 *
 * @param vk the renderer instance created with owl_vk_renderer_init(...)
 * @param model the model instance created with owl_model_init(...)
 * @param matrix the model matrix
 * @return owl_code
 */
owl_public owl_code
owl_vk_draw_model(struct owl_vk_renderer *vk, struct owl_model const *model,
                  owl_m4 const matrix);

owl_public owl_code
owl_vk_draw_renderer_state(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

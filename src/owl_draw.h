#ifndef OWL_DRAW_H
#define OWL_DRAW_H

#include "owl_definitions.h"

OWL_BEGIN_DECLARATIONS

struct owl_renderer;
struct owl_model;
struct owl_texture;
struct owl_cloth_simulation;
struct owl_fluid_simulation;

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
  struct owl_texture *texture;
};

/**
 * @brief draws a quad to the specifications of the quad struct
 *
 * @param vk the renderer instance created with owl_renderer_init(...)
 * @param quad the quad struct describing a quad
 * @param matrix the model matrix
 * @return owl_code
 */
OWL_PUBLIC owl_code
owl_draw_quad(struct owl_renderer *renderer, struct owl_quad const *quad,
              owl_m4 const matrix);

/**
 * @brief draw the loaded skybox
 *
 * @param vk the renderer instance created with owl_renderer_init(...)
 * @return owl_code
 */
OWL_PUBLIC owl_code
owl_draw_skybox(struct owl_renderer *renderer);

/**
 * @brief draws text using the loaded font
 *
 * @param vk the renderer instance created with owl_renderer_init(...)
 * @param text the text to draw
 * @param position the position of the text in normalized coordinates
 * @param color  the color of the text in normalized values
 * @return owl_code
 */
OWL_PUBLIC owl_code
owl_draw_text(struct owl_renderer *renderer, char const *text,
              owl_v3 const position, owl_v3 const color);

/**
 * @brief draw a model
 *
 * @param vk the renderer instance created with owl_renderer_init(...)
 * @param model the model instance created with owl_model_init(...)
 * @param matrix the model matrix
 * @return owl_code
 */
OWL_PUBLIC owl_code
owl_draw_model(struct owl_renderer *renderer, struct owl_model const *model,
               owl_m4 const matrix);

/**
 * @brief draws the renderer debug state
 *
 * @param renderer
 * @return owl_code
 */
OWL_PUBLIC owl_code
owl_draw_renderer_state(struct owl_renderer *renderer);

/**
 * @brief draws the cloth simulation
 *
 * @param renderer
 * @param sim
 * @return OWL_PUBLIC
 */
OWL_PUBLIC owl_code
owl_draw_cloth_simulation(struct owl_renderer *renderer,
                          struct owl_cloth_simulation *sim);
/**
 * @brief
 *
 * @param renderer
 * @param sim
 * @return OWL_PUBLIC
 */
OWL_PUBLIC owl_code
owl_draw_fluid_simulation(struct owl_renderer *renderer,
                          struct owl_fluid_simulation *sim);

OWL_END_DECLARATIONS

#endif

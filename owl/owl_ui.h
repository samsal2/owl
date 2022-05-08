#ifndef OWL_UI_H_
#define OWL_UI_H_

#include "owl_model.h"

struct owl_renderer;
struct owl_model;
struct owl_camera;

struct owl_ui_renderer_state {
  struct owl_font const *font;
};

struct owl_ui_model_state {
  struct owl_font const *font;
};

enum owl_code owl_ui_renderer_state_init(struct owl_renderer *r,
                                         struct owl_font const *f,
                                         struct owl_ui_renderer_state *rs);

void owl_ui_renderer_state_deinit(struct owl_renderer *r,
                                  struct owl_ui_renderer_state *rs);

enum owl_code owl_ui_model_state_init(struct owl_renderer const *r,
                                      struct owl_model const *m,
                                      struct owl_ui_model_state *rs);

void owl_ui_model_state_deinit(struct owl_renderer const *r,
                               struct owl_model const *m,
                               struct owl_ui_model_state *rs);

void owl_ui_renderer_stats_draw(struct owl_ui_renderer_state *rs,
                                struct owl_renderer *r, struct owl_camera *cam);

void owl_ui_model_stats_draw(struct owl_ui_model_state *ms,
                             struct owl_model *m);

#endif

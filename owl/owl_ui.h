#ifndef OWL_UI_H_
#define OWL_UI_H_

#include "owl_model.h"

struct owl_renderer;
struct owl_model;
struct owl_camera;

void owl_ui_renderer_stats_draw(struct owl_renderer *r);

void owl_ui_model_stats_draw(struct owl_renderer *r, struct owl_model *m);

#endif

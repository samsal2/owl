#ifndef OWL_UI_H_
#define OWL_UI_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_renderer;
struct owl_model;
struct owl_camera;

owl_public void
owl_ui_renderer_stats_draw (struct owl_renderer *r);

owl_public void
owl_ui_model_stats_draw (struct owl_renderer *r, struct owl_model *m);

OWL_END_DECLS

#endif

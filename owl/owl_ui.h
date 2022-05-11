#ifndef OWL_UI_H_
#define OWL_UI_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;
struct owl_model;

owl_public void
owl_ui_renderer_stats_draw (struct owl_vk_renderer *r);

owl_public void
owl_ui_model_stats_draw (struct owl_vk_renderer *r, struct owl_model *m);

OWL_END_DECLS

#endif

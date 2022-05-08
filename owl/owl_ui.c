#include "owl_ui.h"

#include "owl_model.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

void owl_ui_begin(struct owl_renderer *r) {
  OWL_UNUSED(r);
}

void owl_ui_end(struct owl_renderer *r) {
  OWL_UNUSED(r);
}

void owl_ui_renderer_stats_draw(struct owl_ui_renderer_state *rs,
                                struct owl_renderer *r) {
  OWL_UNUSED(rs);
  OWL_UNUSED(r);
}

void owl_ui_model_stats_draw(struct owl_ui_model_state *ms,
                             struct owl_model *m) {
  OWL_UNUSED(ms);
  OWL_UNUSED(m);
}

enum owl_code owl_ui_renderer_state_init(struct owl_renderer *r,
                                         struct owl_ui_renderer_state *rs) {
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);
  OWL_UNUSED(rs);

  return code;
}

void owl_ui_renderer_state_deinit(struct owl_renderer *r,
                                  struct owl_ui_renderer_state *rs) {
  OWL_UNUSED(r);
  OWL_UNUSED(rs);
}

enum owl_code owl_ui_model_state_init(struct owl_renderer const *r,
                                      struct owl_model const *m,
                                      struct owl_ui_model_state *ms) {
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);
  OWL_UNUSED(m);
  OWL_UNUSED(ms);

  return code;
}

void owl_ui_model_state_deinit(struct owl_renderer const *r,
                               struct owl_model const *m,
                               struct owl_ui_model_state *ms) {
  OWL_UNUSED(r);
  OWL_UNUSED(m);
  OWL_UNUSED(ms);
}

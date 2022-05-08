#include "owl_ui.h"

#include "owl_draw_command.h"
#include "owl_model.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#include <stdio.h>

void owl_ui_renderer_stats_draw(struct owl_ui_renderer_state *rs,
                                struct owl_renderer *r,
                                struct owl_camera *cam) {
  char buffer[256];
  struct owl_draw_command_text cmd;

  snprintf(buffer, sizeof(buffer), "framerate: %.2f fps",
           1.0 / r->time_stamp_delta);

  cmd.scale = 1.0F;
  cmd.color[0] = 1.0F;
  cmd.color[1] = 1.0F;
  cmd.color[2] = 1.0F;
  cmd.position[0] = -1.0F;
  cmd.position[1] = -0.98F;
  cmd.position[2] = 0.0F;
  cmd.text = buffer;
  cmd.font = rs->font;

  owl_draw_command_text_submit(&cmd, r, cam);

  if ((r->active_frame - 1) < 0) {
    snprintf(buffer, sizeof(buffer), "frame_heap_offset: %llu bytes",
             r->frame_heap_offsets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT - 1]);
  } else {
    snprintf(buffer, sizeof(buffer), "frame_heap_offset: %llu bytes",
             r->frame_heap_offsets[r->active_frame - 1]);
  }

  cmd.position[0] = -1.0F;
  cmd.position[1] = -0.98F * (1 - (0.08 * 1));
  cmd.position[2] = 0.0F;
  cmd.text = buffer;

  owl_draw_command_text_submit(&cmd, r, cam);

  snprintf(buffer, sizeof(buffer), "frame_heap_buffer_size: %llu bytes",
           r->frame_heap_buffer_size);

  cmd.position[0] = -1.0F;
  cmd.position[1] = -0.98F * (1 - (0.08 * 2));
  cmd.position[2] = 0.0F;
  cmd.text = buffer;

  owl_draw_command_text_submit(&cmd, r, cam);
}

void owl_ui_model_stats_draw(struct owl_ui_model_state *ms,
                             struct owl_model *m) {
  OWL_UNUSED(ms);
  OWL_UNUSED(m);
}

enum owl_code owl_ui_renderer_state_init(struct owl_renderer *r,
                                         struct owl_font const *f,
                                         struct owl_ui_renderer_state *rs) {
  enum owl_code code = OWL_SUCCESS;

  OWL_UNUSED(r);

  rs->font = f;

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

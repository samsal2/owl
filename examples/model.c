#include <owl/owl.h>

#include <stdio.h>
#include <stdlib.h>

static struct owl_window *window;
static struct owl_renderer *renderer;
static struct owl_model *model;
static owl_m4 matrix;

#define MODEL_PATH "../../assets/CesiumMan.gltf"

char const *
fmtfps (double d) {
  static char buffer[128];
  snprintf (buffer, sizeof (buffer), "%.2f fps", d);
  return buffer;
}

#define CHECK(fn)                                                             \
  do {                                                                        \
    enum owl_code code = (fn);                                                \
    if (OWL_SUCCESS != (code)) {                                              \
      printf ("something went wrong in call: %s, code %i\n", (#fn), code);    \
      return 0;                                                               \
    }                                                                         \
  } while (0)

int
main (void) {
  window = malloc (sizeof (*window));
  CHECK (owl_window_init (window, 600, 600, "model"));

  renderer = malloc (sizeof (*renderer));
  CHECK (owl_renderer_init (renderer, window));

  model = malloc (sizeof (*model));
  CHECK (owl_model_init (model, renderer, MODEL_PATH));

  owl_m4_identity (matrix);
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate (offset, matrix);
  }

  while (!owl_window_is_done (window)) {
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_begin (renderer)) {
      owl_window_handle_resize (window);
      owl_renderer_swapchain_resize (renderer, window);
      continue;
    }

    owl_model_anim_update (model, 0, renderer->active_frame,
                           renderer->time_stamp_delta);

    owl_renderer_bind_pipeline (renderer, OWL_RENDERER_PIPELINE_MODEL);
    owl_renderer_model_draw (renderer, model, matrix);

    owl_renderer_bind_pipeline (renderer, OWL_RENDERER_PIPELINE_FONT);
    owl_ui_renderer_stats_draw (renderer);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_end (renderer)) {
      owl_window_handle_resize (window);
      owl_renderer_swapchain_resize (renderer, window);
      continue;
    }

    owl_window_poll_events (window);
  }

  owl_model_deinit (model, renderer);
  free (model);

  owl_renderer_deinit (renderer);
  free (renderer);

  owl_window_deinit (window);
  free (window);
}

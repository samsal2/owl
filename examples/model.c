#include "owl/owl_renderer.h"
#include <owl/owl.h>

#include <stdio.h>
#include <stdlib.h>

static struct owl_window_init_info window_info;
static struct owl_window *window;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer *renderer;
static struct owl_model *model;
static owl_m4 matrix;
static struct owl_camera camera;

#define MODEL_PATH "../../assets/CesiumMan.gltf"

char const *fmtfps(double d) {
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "%.2f fps", d);
  return buffer;
}

#define CHECK(fn)                                                              \
  do {                                                                         \
    enum owl_code code = (fn);                                                 \
    if (OWL_SUCCESS != (code)) {                                               \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);      \
      return 0;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  window_info.height = 600;
  window_info.width = 600;
  window_info.title = "model";
  window = OWL_MALLOC(sizeof(*window));
  CHECK(owl_window_init(window, &window_info));

  CHECK(owl_window_fill_renderer_init_info(window, &renderer_info));
  renderer = OWL_MALLOC(sizeof(*renderer));
  CHECK(owl_renderer_init(renderer, &renderer_info));

  CHECK(owl_camera_init(&camera));

  model = OWL_MALLOC(sizeof(*model));
  CHECK(owl_model_init(model, renderer, MODEL_PATH));

  OWL_M4_IDENTITY(matrix);
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate(offset, matrix);
  }

  while (!owl_window_is_done(window)) {
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_begin(renderer)) {
      owl_window_handle_resize(window);
      owl_window_fill_renderer_init_info(window, &renderer_info);
      owl_renderer_swapchain_resize(renderer, &renderer_info);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_model_anim_update(model, 0, renderer->active_frame,
                          renderer->time_stamp_delta);

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_MODEL);
    owl_renderer_model_draw(renderer, model, matrix);

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_FONT);
    owl_ui_renderer_stats_draw(renderer);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_end(renderer)) {
      owl_window_handle_resize(window);
      owl_window_fill_renderer_init_info(window, &renderer_info);
      owl_renderer_swapchain_resize(renderer, &renderer_info);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_window_poll_events(window);
  }

  owl_camera_deinit(&camera);

  owl_model_deinit(model, renderer);
  OWL_FREE(model);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_window_deinit(window);
  OWL_FREE(window);
}

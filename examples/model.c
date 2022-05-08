#include <owl/owl.h>

#include <stdio.h>
#include <stdlib.h>

static struct owl_window_init_desc window_desc;
static struct owl_window *window;
static struct owl_renderer_init_desc renderer_desc;
static struct owl_renderer *renderer;
static struct owl_ui_renderer_state renderer_ui;
static struct owl_model *model;
static struct owl_ui_model_state model_ui;
static struct owl_draw_command_model model_command;
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
  window_desc.height = 600;
  window_desc.width = 600;
  window_desc.title = "model";
  window = OWL_MALLOC(sizeof(*window));
  CHECK(owl_window_init(&window_desc, window));

  CHECK(owl_window_fill_renderer_init_desc(window, &renderer_desc));
  renderer = OWL_MALLOC(sizeof(*renderer));
  CHECK(owl_renderer_init(&renderer_desc, renderer));

  CHECK(owl_camera_init(&camera));

  model = OWL_MALLOC(sizeof(*model));
  CHECK(owl_model_init(renderer, MODEL_PATH, model));

  CHECK(owl_ui_renderer_state_init(renderer, &renderer_ui));
  CHECK(owl_ui_model_state_init(renderer, model, &model_ui));

  OWL_V3_SET(0.0F, 0.0F, 1.9F, model_command.light);
  OWL_M4_IDENTITY(model_command.model);
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate(offset, model_command.model);
  }
  model_command.skin = model;

  OWL_V3_SET(0.0F, 1.0F, 0.0F, model_command.light);

  while (!owl_window_is_done(window)) {
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_begin(renderer)) {
      owl_window_handle_resize(window);
      owl_window_fill_renderer_init_desc(window, &renderer_desc);
      owl_renderer_swapchain_resize(&renderer_desc, renderer);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }


#if 1
    owl_model_update_animation(model, 0, renderer->active_frame,
                               renderer->time_stamp_delta);
#endif

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_MODEL);
    owl_draw_command_model_submit(&model_command, renderer, &camera);

    owl_ui_begin(renderer);
    {
      owl_ui_renderer_stats_draw(&renderer_ui, renderer);
      owl_ui_model_stats_draw(&model_ui, model);
    }
    owl_ui_end(renderer);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_end(renderer)) {
      owl_window_handle_resize(window);
      owl_window_fill_renderer_init_desc(window, &renderer_desc);
      owl_renderer_swapchain_resize(&renderer_desc, renderer);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_window_poll_events(window);
  }

  owl_ui_model_state_deinit(renderer, model, &model_ui);
  owl_ui_renderer_state_deinit(renderer, &renderer_ui);

  owl_camera_deinit(&camera);

  owl_model_deinit(renderer, model);
  OWL_FREE(model);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_window_deinit(window);
  OWL_FREE(window);
}

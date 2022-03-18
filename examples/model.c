#include <owl/owl.h>

#include <stdio.h>

static struct owl_client_init_info client_info;
static struct owl_client *client;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer *renderer;
static struct owl_draw_model_command model_command;
static struct owl_camera camera;
static struct owl_font *font;
static struct owl_draw_text_command text_command;

#define MODEL_PATH "../../assets/CesiumMan.gltf"
#define FONT_PATH "../../assets/Inconsolata-Regular.ttf"

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
  struct owl_model *model;

  client_info.height = 600;
  client_info.width = 600;
  client_info.title = "model";
  client = OWL_MALLOC(sizeof(*client));
  CHECK(owl_client_init(&client_info, client));

  CHECK(owl_client_fill_renderer_init_info(client, &renderer_info));
  renderer = OWL_MALLOC(sizeof(*renderer));
  CHECK(owl_renderer_init(&renderer_info, renderer));

  CHECK(owl_camera_init(&camera));

  model = OWL_MALLOC(sizeof(*model));
  CHECK(owl_model_init(renderer, MODEL_PATH, model));

  OWL_V3_SET(0.0F, 0.0F, 1.9F, model_command.light);
  model_command.model = model;

  font = OWL_MALLOC(sizeof(*font));
  CHECK(owl_font_init(renderer, 64, FONT_PATH, font));

  OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);
  OWL_V3_SET(-0.3F, -0.3F, 0.90, text_command.position);
  text_command.font = font;
  text_command.scale = 0.05F;

  while (!owl_client_is_done(client)) {
    OWL_V2_COPY(client->cursor_position, model_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      continue;
    }

#if 1
    owl_model_update_animation(renderer, model, client->dt_time_stamp);
#endif

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MODEL);
    owl_submit_draw_model_command(renderer, &camera, &model_command);

#if 1
    text_command.text = fmtfps(client->fps);
    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
    owl_submit_draw_text_command(renderer, &camera, &text_command);
#endif

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_font_deinit(renderer, font);
  OWL_FREE(font);

  owl_camera_deinit(&camera);

  owl_model_deinit(renderer, model);
  OWL_FREE(model);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_client_deinit(client);
  OWL_FREE(client);
}

#include "owl/imgui.h"
#include <owl/owl.h>

#include <stdio.h>

static struct owl_client_init_info client_info;
static struct owl_client *client;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer *renderer;
static struct owl_model *model;
static struct owl_draw_model_command model_command;
static struct owl_camera camera;

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

  CHECK(owl_imgui_init(client, renderer, FONT_PATH));

  while (!owl_client_is_done(client)) {
    OWL_V2_COPY(client->cursor_position, model_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      owl_imgui_handle_resize(client, renderer);
      continue;
    }

#if 1
    owl_model_update_animation(renderer, model, client->dt_time_stamp);
#endif

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MODEL);
    owl_submit_draw_model_command(renderer, &camera, &model_command);

    owl_imgui_begin_frame(renderer);
    owl_imgui_begin("hello world");
    owl_imgui_text("hi %.8f", (float)client->dt_time_stamp);
    owl_imgui_end();
    owl_imgui_end_frame(renderer);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      owl_imgui_handle_resize(client, renderer);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_imgui_deinit(renderer);

  owl_camera_deinit(&camera);

  owl_model_deinit(renderer, model);
  OWL_FREE(model);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_client_deinit(client);
  OWL_FREE(client);
}

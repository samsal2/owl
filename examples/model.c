#include <owl/owl.h>

#include <stdio.h>
#include <stdlib.h>

static struct owl_client_init_desc client_desc;
static struct owl_client *client;
static struct owl_renderer_init_desc renderer_desc;
static struct owl_renderer *renderer;
static struct owl_model *model;
static struct owl_draw_command_model model_command;
static struct owl_camera camera;
static struct owl_font *font;
static struct owl_draw_command_text text_command;

#define MODEL_PATH "../assets/CesiumMan.gltf"
#define FONT_PATH "../assets/Inconsolata-Regular.ttf"

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
  client_desc.height = 600;
  client_desc.width = 600;
  client_desc.title = "model";
  client = OWL_MALLOC(sizeof(*client));
  CHECK(owl_client_init(&client_desc, client));

  CHECK(owl_client_fill_renderer_init_desc(client, &renderer_desc));
  renderer = OWL_MALLOC(sizeof(*renderer));
  CHECK(owl_renderer_init(&renderer_desc, renderer));

  CHECK(owl_camera_init(&camera));

  model = OWL_MALLOC(sizeof(*model));
  CHECK(owl_model_init(renderer, MODEL_PATH, model));

  OWL_V3_SET(0.0F, 0.0F, 1.9F, model_command.light);
  OWL_M4_IDENTITY(model_command.model);
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate(offset, model_command.model);
  }
  model_command.skin = model;

  font = OWL_MALLOC(sizeof(*font));
  CHECK(owl_font_init(renderer, 78, FONT_PATH, font));

  OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);
  OWL_V3_SET(-0.8F, -0.8F, 0.90, text_command.position);
  text_command.font = font;
  text_command.scale = 20.0F;

  while (!owl_client_is_done(client)) {
    OWL_V2_COPY(client->cursor_position, model_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_desc);
      owl_renderer_swapchain_resize(&renderer_desc, renderer);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }

    if (OWL_BUTTON_STATE_PRESS ==
        client->mouse_buttons[OWL_MOUSE_BUTTON_LEFT]) {
      owl_v3 side = {1.0F, 0.0F, 0.0F};
      owl_v3 up = {0.0F, 1.0F, 0.0F};
      owl_m4_rotate(model_command.model,
                    -client->delta_cursor_position[1] * 2.0F, side,
                    model_command.model);
      owl_m4_rotate(model_command.model,
                    client->delta_cursor_position[0] * 2.0F, up,
                    model_command.model);
    }

#if 1
    owl_model_update_animation(model, 0, renderer->active_frame,
                               client->delta_time_stamp);
#endif

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_MODEL);
    owl_draw_command_model_submit(&model_command, renderer, &camera);

#if 1
    text_command.text = fmtfps(client->fps);
    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_FONT);
    owl_draw_command_text_submit(&text_command, renderer, &camera);
#endif

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_desc);
      owl_renderer_swapchain_resize(&renderer_desc, renderer);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
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

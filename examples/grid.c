#include <owl/owl.h>

#include <stdio.h>

static struct owl_client_init_desc client_desc;
static struct owl_client *client;
static struct owl_renderer_init_desc renderer_desc;
static struct owl_renderer *renderer;
static struct owl_camera camera;
static struct owl_font *font;
static struct owl_draw_command_text text_command;
static struct owl_draw_command_grid grid_command;

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
  client_desc.height = 600;
  client_desc.width = 600;
  client_desc.title = "grid";
  client = OWL_MALLOC(sizeof(*client));
  CHECK(owl_client_init(&client_desc, client));

  CHECK(owl_client_fill_renderer_init_desc(client, &renderer_desc));
  renderer = OWL_MALLOC(sizeof(*renderer));
  CHECK(owl_renderer_init(&renderer_desc, renderer));

  CHECK(owl_camera_init(&camera));

  font = OWL_MALLOC(sizeof(*font));
  CHECK(owl_font_init(renderer, 64, FONT_PATH, font));

  OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);
  OWL_V3_SET(-0.3F, -0.3F, 0.90, text_command.position);
  text_command.font = font;
  text_command.scale = 0.05F;

  while (!owl_client_is_done(client)) {
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_desc);
      owl_renderer_resize_swapchain(&renderer_desc, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      continue;
    }

#if 1
    text_command.text = fmtfps(client->fps);
    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_TYPE_FONT);
    owl_draw_command_submit_text(renderer, &camera, &text_command);
#endif

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_desc);
      owl_renderer_resize_swapchain(&renderer_desc, renderer);
      owl_camera_set_ratio(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_font_deinit(renderer, font);
  OWL_FREE(font);

  owl_camera_deinit(&camera);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_client_deinit(client);
  OWL_FREE(client);
}

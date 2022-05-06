#include "owl/owl_imgui.h"
#include "owl/owl_renderer.h"
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
static struct owl_imgui imgui;

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

#if 0
  font = OWL_MALLOC(sizeof(*font));
  CHECK(owl_font_init(renderer, 64, "../../assets/Inconsolata-Regular.ttf",
                      font));
#else
  OWL_UNUSED(font);
#endif

  OWL_V3_SET(0.0F, 0.0F, 1.9F, model_command.light);
  OWL_M4_IDENTITY(model_command.model);
  {
    owl_v3 offset = {0.0F, 0.0F, -1.0F};
    owl_m4_translate(offset, model_command.model);
  }
  model_command.skin = model;

  CHECK(owl_imgui_init(client, renderer, &imgui));

  while (!owl_client_is_done(client)) {
    OWL_V2_COPY(client->cursor_position, model_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_begin(renderer)) {
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
    owl_imgui_begin_frame(&imgui);
    {
      owl_v2 position = {-1.0F, -1.0F};
      owl_v2 pivot = {0.0F, 0.0F};
      owl_v2 size = {150.0F, client->window_height};

      owl_imgui_set_next_window_position(&imgui, position, 0, pivot);
      owl_imgui_set_next_window_size(&imgui, size, 0);

      owl_imgui_begin(&imgui, "Hello world!",
                      OWL_IMGUI_WINDOW_FLAGS_NO_MOVE |
                          OWL_IMGUI_WINDOW_FLAGS_NO_RESIZE |
                          OWL_IMGUI_WINDOW_FLAGS_NO_COLLAPSE |
                          OWL_IMGUI_WINDOW_FLAGS_NO_TITLE_BAR);

      owl_imgui_text(&imgui, "Framerate %.2F", owl_imgui_framerate(&imgui));

      owl_imgui_end(&imgui);

      owl_imgui_demo_window(&imgui);
    }
    owl_imgui_render(&imgui, renderer);
    owl_imgui_end_frame(&imgui);
#endif

#if 0
    text_command.scale = 1.0F;
    text_command.text = fmtfps(1 / client->delta_time_stamp);
    text_command.font = font;
    OWL_V3_SET(-0.9F, -0.9F, 0.0F, text_command.position);
    OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_FONT);
    owl_draw_command_text_submit(&text_command, renderer, &camera);
#else
    OWL_UNUSED(text_command);
#endif

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_end(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_desc);
      owl_renderer_swapchain_resize(&renderer_desc, renderer);
      owl_camera_ratio_set(&camera, renderer->framebuffer_ratio);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_imgui_deinit(renderer, &imgui);

#if 0
  owl_font_deinit(renderer, font);
  OWL_FREE(font);
#endif

  owl_camera_deinit(&camera);

  owl_model_deinit(renderer, model);
  OWL_FREE(model);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_client_deinit(client);
  OWL_FREE(client);
}

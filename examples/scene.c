#include "owl/camera.h"
#include "owl/renderer.h"
#include <owl/owl.h>

#include <stdio.h>

static struct owl_client_init_info client_info;
static struct owl_client *client;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer *renderer;
static struct owl_scene *scene;
static struct owl_draw_scene_command scene_command;
static struct owl_camera cam;
static struct owl_font *font;
static struct owl_draw_text_command text_command;

#if 1
#define SCENE_PATH "../../assets/CesiumMan.gltf"
#else
#define SCENE_PATH "../../assets/Suzanne.gltf"
#endif

#define FONT_PATH "../../assets/Inconsolata-Regular.ttf"

#define TEST(fn)                                                               \
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
  client_info.title = "scene";
  client = OWL_MALLOC(sizeof(*client));
  TEST(owl_client_init(&client_info, client));

  TEST(owl_client_fill_renderer_init_info(client, &renderer_info));
  renderer = OWL_MALLOC(sizeof(*renderer));
  TEST(owl_renderer_init(&renderer_info, renderer));

  TEST(owl_camera_init(&cam));

  scene = OWL_MALLOC(sizeof(*scene));
  TEST(owl_scene_init(renderer, SCENE_PATH, scene));

  OWL_V3_SET(0.0F, 0.0F, -1.5F, scene_command.light);
  scene_command.scene = scene;

  font = OWL_MALLOC(sizeof(*font));
  TEST(owl_font_init(renderer, 80, FONT_PATH, font));

  OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);
  OWL_V3_SET(-0.1F, -0.1F, 0.90, text_command.position);
  text_command.text = "HELLO WORLD!";
  text_command.font = font;

  while (!owl_client_is_done(client)) {
    OWL_V2_COPY(client->cursor_position, scene_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&cam, (float)renderer->framebuffer_width /
                                     (float)renderer->framebuffer_height);
      continue;
    }

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_SCENE);
    owl_submit_draw_scene_command(renderer, &cam, &scene_command);

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
    owl_submit_draw_text_command(renderer, &cam, &text_command);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_info(client, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, renderer);
      owl_camera_set_ratio(&cam, (float)renderer->framebuffer_width /
                                     (float)renderer->framebuffer_height);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_font_deinit(renderer, font);
  OWL_FREE(font);

  owl_camera_deinit(&cam);

  owl_scene_deinit(renderer, scene);
  OWL_FREE(scene);

  owl_renderer_deinit(renderer);
  OWL_FREE(renderer);

  owl_client_deinit(client);
  OWL_FREE(client);
}

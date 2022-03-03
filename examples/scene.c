#include <owl/owl.h>

#include <stdio.h>

static struct owl_window_init_info window_info;
static struct owl_window window;
static struct owl_input_state *input;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer renderer;
static struct owl_scene scene;
static struct owl_draw_scene_command scene_command;
static struct owl_camera cam;

#define SCENE_PATH "../../assets/Suzanne.gltf"

#define TEST(fn)                                                               \
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
  window_info.title = "scene";
  TEST(owl_window_init(&window_info, &input, &window));

  TEST(owl_window_fill_renderer_init_info(&window, &renderer_info));
  TEST(owl_renderer_init(&renderer_info, &renderer));

  TEST(owl_camera_init(&cam));

  TEST(owl_scene_init(&renderer, SCENE_PATH, &scene));

  OWL_V3_SET(0.0F, 0.0F, -1.5F, scene_command.light);
  scene_command.scene = &scene;

  while (!owl_window_is_done(&window)) {
    OWL_V2_COPY(input->cursor_position, scene_command.light);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(&renderer)) {
      owl_window_fill_renderer_init_info(&window, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, &renderer);
      continue;
    }

    owl_renderer_bind_pipeline(&renderer, OWL_PIPELINE_TYPE_SCENE);
    owl_submit_draw_scene_command(&renderer, &cam, &scene_command);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(&renderer)) {
      owl_window_fill_renderer_init_info(&window, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, &renderer);
      continue;
    }

    owl_window_poll(&window);
  }

  owl_camera_deinit(&cam);
  owl_scene_deinit(&renderer, &scene);
  owl_renderer_deinit(&renderer);
  owl_window_deinit(&window);
}

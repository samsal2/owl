#include <owl/owl.h>
#include <stdio.h>

static struct owl_window_init_info window_info;
static struct owl_input_state *input;
static struct owl_window window;
static struct owl_renderer_init_info renderer_info;
static struct owl_renderer renderer;
static struct owl_camera camera;
static struct owl_model model;

#define TEST(fn)                                                                                                       \
  do {                                                                                                                 \
    enum owl_code code = (fn);                                                                                         \
    if (OWL_SUCCESS != (code)) {                                                                                       \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);                                              \
      return 0;                                                                                                        \
    }                                                                                                                  \
  } while (0)

int main(void) {
  window_info.height = 600;
  window_info.width = 600;
  window_info.title = "model";
  TEST(owl_window_init(&window_info, &input, &window));

  TEST(owl_window_fill_renderer_init_info(&window, &renderer_info));
  TEST(owl_renderer_init(&renderer_info, &renderer));

  TEST(owl_camera_init(&camera));
  TEST(owl_model_init(&renderer, "../../assets/Suzanne.gltf", &model));

  while (!owl_window_is_done(&window)) {
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(&renderer)) {
      owl_window_fill_renderer_init_info(&window, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, &renderer);
      continue;
    }

    owl_renderer_bind_pipeline(&renderer, OWL_PIPELINE_TYPE_PBR);
    owl_model_submit(&renderer, &camera, &model);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(&renderer)) {
      owl_window_fill_renderer_init_info(&window, &renderer_info);
      owl_renderer_resize_swapchain(&renderer_info, &renderer);
      continue;
    }

    owl_window_poll(&window);
  }

  owl_model_deinit(&renderer, &model);
  owl_renderer_deinit(&renderer);
  owl_window_deinit(&window);
}

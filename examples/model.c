#include <owl.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static double prev_time_stamp;
static double time_stamp;
static struct owl_plataform *window;
static struct owl_renderer *renderer;
static struct owl_model *model;
static owl_m4 matrix;

#define CHECK(fn)                                                             \
  do {                                                                        \
    int code = (fn);                                                          \
    if (code) {                                                               \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);     \
      return 0;                                                               \
    }                                                                         \
  } while (0)

int main(void) {
  owl_v3 offset = {0.0F, 0.0F, -1.0F};

  window = malloc(sizeof(*window));
  CHECK(owl_plataform_init(window, 600, 600, "model"));

  renderer = malloc(sizeof(*renderer));
  CHECK(owl_renderer_init(renderer, window));

  model = malloc(sizeof(*model));
  CHECK(owl_model_init(model, renderer, "../../assets/CesiumMan.gltf"));

  CHECK(owl_renderer_load_font(renderer, 64.0F,
                               "../../assets/CascadiaMono.ttf"));

  CHECK(owl_renderer_load_skybox(renderer, "../../assets/skybox"));

  OWL_V4_IDENTITY(matrix);
  owl_m4_translate(offset, matrix);

  prev_time_stamp = 0;
  time_stamp = 0;

  while (!owl_plataform_should_close(window)) {
    owl_v3 axis = {1.0F, 0.0F, 0.0F};

    prev_time_stamp = time_stamp;
    time_stamp = owl_plataform_get_time(window);
#if 0
    owl_camera_rotate (&renderer->camera, axis, 0.01);
#else
    (void)(axis);
#endif

    CHECK(owl_renderer_begin_frame(renderer));

    owl_draw_skybox(renderer);

    owl_model_anim_update(model, renderer->frame, time_stamp - prev_time_stamp,
                          0);

    owl_draw_model(renderer, model, matrix);
    owl_draw_renderer_state(renderer);

    CHECK(owl_renderer_end_frame(renderer));

    owl_plataform_poll_events(window);
  }

  owl_model_deinit(model, renderer);
  free(model);

  owl_renderer_deinit(renderer);
  free(renderer);

  owl_plataform_deinit(window);
  free(window);
}

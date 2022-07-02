#include <owl.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double prev_time_stamp;
static double time_stamp;
static struct owl_plataform *window;
static struct owl_renderer *renderer;
static struct owl_fluid_simulation *simulation;
static owl_m4 matrix;

#define CHECK(fn)                                                             \
  do {                                                                        \
    owl_code code = (fn);                                                     \
    if (code) {                                                               \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);     \
      return 0;                                                               \
    }                                                                         \
  } while (0)

int
main(void) {
  owl_v3 offset = {0.0F, 0.0F, -1.0F};

  window = malloc(sizeof(*window));
  CHECK(owl_plataform_init(window, 600, 600, "model"));

  renderer = malloc(sizeof(*renderer));
  CHECK(owl_renderer_init(renderer, window));

  simulation = malloc(sizeof(*simulation));
  CHECK(owl_fluid_simulation_init(simulation, renderer));

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

#if 0
    owl_draw_skybox(renderer);
#endif

    owl_fluid_simulation_update(simulation, renderer,
                                time_stamp - prev_time_stamp);

    owl_draw_renderer_state(renderer);
    owl_draw_fluid_simulation(renderer, simulation);

    CHECK(owl_renderer_end_frame(renderer));

    owl_plataform_poll_events(window);
  }

  owl_fluid_simulation_deinit(simulation, renderer);
  free(simulation);

  owl_renderer_deinit(renderer);
  free(renderer);

  owl_plataform_deinit(window);
  free(window);
}

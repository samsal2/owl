#include <owl/owl.h>

#include <stdio.h>

#define TEST(fn)                                                             \
  do {                                                                       \
    if (OWL_SUCCESS != (fn)) {                                               \
      printf("something went wrong in call: %s\n", (#fn));                   \
      return 0;                                                              \
    }                                                                        \
  } while (0)

static struct owl_window *window;
static struct owl_renderer *renderer;

int main(void) {
  TEST(owl_create_window(600, 600, "fluid-simulation", &window));
  TEST(owl_create_renderer(window, &renderer));

  while (!owl_should_window_close(window)) {
    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      owl_recreate_renderer(window, renderer);
      continue;
    }

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      owl_recreate_renderer(window, renderer);
      continue;
    }

    owl_poll_events(window);
  }

  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}

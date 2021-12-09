#ifndef OWL_WINDOW_H_
#define OWL_WINDOW_H_

#include <owl/types.h>

struct owl_vk_surface_provider;
struct owl_vk_extensions;

typedef double OwlSeconds;
typedef void *OwlWindowHandle;

enum owl_btn_state {
  OWL_BUTTON_STATE_NONE,
  OWL_BUTTON_STATE_PRESS,
  OWL_BUTTON_STATE_RELEASE
};

enum owl_mouse_btn {
  OWL_MOUSE_BUTTON_LEFT,
  OWL_MOUSE_BUTTON_MIDDLE,
  OWL_MOUSE_BUTTON_RIGHT,
  OWL_MOUSE_BUTTON_COUNT
};

struct owl_window {
  OwlV2 cursor;
  OwlWindowHandle handle;
  struct owl_extent size;
  struct owl_extent framebuffer;
  enum owl_btn_state mouse[OWL_MOUSE_BUTTON_COUNT];
};

struct owl_timer {
  OwlSeconds start;
  OwlSeconds end;
};

enum owl_code owl_create_window(int width, int height, char const *title,
                                struct owl_window **window);

void owl_destroy_window(struct owl_window *window);

int owl_should_window_close(struct owl_window const *window);

void owl_poll_events(struct owl_window const *window);

void owl_start_timer(struct owl_timer *timer);

OwlSeconds owl_end_timer(struct owl_timer *timer);

#endif

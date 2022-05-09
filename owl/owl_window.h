#ifndef OWL_CLIENT_H_
#define OWL_CLIENT_H_

#include "owl_types.h"

struct owl_renderer_init_info;

struct owl_io;

struct owl_window_init_info {
  char const *title;
  owl_i32 width;
  owl_i32 height;
};

struct owl_window {
  char const *title;
  void *data;
  float framebuffer_ratio;
  owl_i32 framebuffer_width;
  owl_i32 framebuffer_height;
  owl_i32 window_width;
  owl_i32 window_height;
};

enum owl_code owl_window_init(struct owl_window *w,
                              struct owl_window_init_info const *info);

void owl_window_deinit(struct owl_window *w);

enum owl_code
owl_window_fill_renderer_init_info(struct owl_window const *w,
                                   struct owl_renderer_init_info *info);

void owl_window_poll_events(struct owl_window *w);

owl_i32 owl_window_is_done(struct owl_window *w);

void owl_window_handle_resize(struct owl_window *w);

void owl_window_bind_io(struct owl_window *w, struct owl_io *io);

#endif

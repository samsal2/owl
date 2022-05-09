#ifndef OWL_WINDOW_H_
#define OWL_WINDOW_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_renderer_init_info;

struct owl_io;

struct owl_window_init_info {
  char const *title;
  owl_i32     width;
  owl_i32     height;
};

struct owl_window {
  char const *title;
  void       *data;
  owl_i32     window_width;
  owl_i32     window_height;
  float       framebuffer_ratio;
  owl_i32     framebuffer_width;
  owl_i32     framebuffer_height;
};

owl_public enum owl_code
owl_window_init (struct owl_window *w, struct owl_window_init_info const *info);

owl_public void
owl_window_deinit (struct owl_window *w);

owl_public enum owl_code
owl_window_fill_renderer_init_info (struct owl_window const       *w,
                                    struct owl_renderer_init_info *info);

owl_public void
owl_window_poll_events (struct owl_window *w);

owl_public owl_i32
owl_window_is_done (struct owl_window *w);

owl_public void
owl_window_handle_resize (struct owl_window *w);

owl_public void
owl_window_bind_io (struct owl_window *w, struct owl_io *io);

OWL_END_DECLS

#endif

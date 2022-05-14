#ifndef OWL_WINDOW_H_
#define OWL_WINDOW_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_renderer;

struct owl_window {
  char const *title;
  void *opaque;
};

owl_public enum owl_code
owl_window_init (struct owl_window *w, owl_i32 width, owl_i32 height,
                 char const *name);

owl_public void
owl_window_deinit (struct owl_window *w);

owl_public void
owl_window_poll_events (struct owl_window *w);

owl_public owl_i32
owl_window_is_done (struct owl_window *w);

owl_public char const *const *
owl_window_get_instance_extensions (owl_u32 *count);

owl_public enum owl_code
owl_window_create_vk_surface (struct owl_window const *w, VkInstance instance,
                              VkSurfaceKHR *surface);
owl_public void
owl_window_get_framebuffer_size (struct owl_window const *w, owl_i32 *width,
                                 owl_i32 *height);
owl_public void
owl_window_get_window_size (struct owl_window const *w, owl_i32 *width,
                            owl_i32 *height);

OWL_END_DECLS

#endif

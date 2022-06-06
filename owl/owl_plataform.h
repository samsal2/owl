#ifndef OWL_PLATAFORM_H_
#define OWL_PLATAFORM_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

struct owl_plataform {
  char const *title;
  void *opaque;
};

owl_public owl_code
owl_plataform_init(struct owl_plataform *plataform, int32_t width,
                   int32_t height, char const *title);

owl_public void
owl_plataform_deinit(struct owl_plataform *plataform);

owl_public owl_code
owl_plataform_get_vulkan_extensions(struct owl_plataform *plataform,
                                    uint32_t *num_extensions,
                                    char const *const **extensions);

owl_public char const *
owl_plataform_get_title(struct owl_plataform const *plataform);

owl_public void
owl_plataform_get_window_dimensions(struct owl_plataform const *plataform,
                                    uint32_t *width, uint32_t *height);

owl_public void
owl_plataform_get_framebuffer_dimensions(struct owl_plataform const *plataform,
                                         uint32_t *width, uint32_t *height);

owl_public owl_code
owl_plataform_create_vulkan_surface(struct owl_plataform *plataform,
                                    struct owl_vk_renderer *vk);

owl_public int32_t
owl_plataform_should_close(struct owl_plataform *plataform);

owl_public void
owl_plataform_poll_events(struct owl_plataform *plataform);

owl_public double
owl_plataform_get_time(struct owl_plataform *plataform);

OWL_END_DECLS

#endif
#ifndef OWL_SOURCE_VK_CONFIG_H_
#define OWL_SOURCE_VK_CONFIG_H_

#include "../include/owl/code.h"
#include "../include/owl/types.h"

struct owl_vk_renderer;

struct owl_vk_config {
  OwlU32 framebuffer_width;
  OwlU32 framebuffer_height;

  OwlU32 instance_extension_count;
  char const **instance_extensions;

  void const *surface_user_data;
  enum owl_code (*create_surface)(struct owl_vk_renderer const *, void const *,
                                  void *);
};

#endif

#ifndef ARES_VK_CONFIG_H_
#define ARES_VK_CONFIG_H_

#include <owl/code.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

struct owl_vk_renderer;

typedef enum owl_code (*OwlVkSurfaceCreator)(void const *,
                                             struct owl_vk_renderer const *,
                                             VkSurfaceKHR *);

struct owl_vk_config {
  OwlU32 framebuffer_width;
  OwlU32 framebuffer_height;

  OwlU32 instance_extension_count;
  char const **instance_extensions;

  void const *surface_user_data;
  OwlVkSurfaceCreator create_surface;
};

#endif

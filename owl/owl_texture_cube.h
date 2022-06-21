#ifndef OWL_TEXTURE_CUBE_H_
#define OWL_TEXTURE_CUBE_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_renderer;

struct owl_texture_cube_desc {
  char const *files[6];
};

struct owl_texture_cube {
  uint32_t width;
  uint32_t height;
  uint32_t mips;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkDescriptorSet set;
  VkImageLayout layout;
};

OWL_PUBLIC owl_code
owl_texture_cube_init(struct owl_texture_cube *texture,
    struct owl_renderer *renderer, struct owl_texture_cube_desc *desc);

OWL_PUBLIC void
owl_texture_cube_deinit(struct owl_texture_cube *texture,
    struct owl_renderer *renderer);

OWL_END_DECLS

#endif

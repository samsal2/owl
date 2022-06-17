#ifndef OWL_TEXTURE_2D_H_
#define OWL_TEXTURE_2D_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_renderer;

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_texture_2d_source {
  OWL_TEXTURE_SOURCE_DATA,
  OWL_TEXTURE_SOURCE_FILE
};

struct owl_texture_2d {
  uint32_t width;
  uint32_t height;
  uint32_t mips;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkDescriptorSet set;
  VkImageLayout layout;
};

struct owl_texture_2d_desc {
  enum owl_texture_2d_source source;
  char const *file;
  uint8_t *data;
  uint32_t width;
  uint32_t height;
  enum owl_pixel_format format;
};

owl_public owl_code
owl_texture_2d_init(struct owl_texture_2d *texture,
                    struct owl_renderer *renderer,
                    struct owl_texture_2d_desc *desc);

owl_public void
owl_texture_2d_deinit(struct owl_texture_2d *texture,
                      struct owl_renderer *renderer);

OWL_END_DECLS

#endif
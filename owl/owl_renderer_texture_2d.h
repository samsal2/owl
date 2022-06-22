#ifndef OWL_RENDERER_TEXTURE_2D_H_
#define OWL_RENDERER_TEXTURE_2D_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

struct owl_renderer;

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_renderer_texture_2d_source {
  OWL_RENDERER_TEXTURE_SOURCE_DATA,
  OWL_RENDERER_TEXTURE_SOURCE_FILE
};

struct owl_renderer_texture_2d {
  uint32_t width;
  uint32_t height;
  uint32_t mips;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkDescriptorSet set;
  VkImageLayout layout;
};

struct owl_renderer_texture_2d_desc {
  enum owl_renderer_texture_2d_source source;
  char const *file;
  uint8_t *data;
  uint32_t width;
  uint32_t height;
  enum owl_pixel_format format;
};

OWL_PUBLIC owl_code owl_renderer_texture_2d_init(
    struct owl_renderer_texture_2d *texture, struct owl_renderer *renderer,
    struct owl_renderer_texture_2d_desc *desc);

OWL_PUBLIC void owl_renderer_texture_2d_deinit(
    struct owl_renderer_texture_2d *texture, struct owl_renderer *renderer);

OWL_END_DECLARATIONS

#endif

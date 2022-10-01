#ifndef OWL_TEXTURE_H
#define OWL_TEXTURE_H

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

struct owl_renderer;

enum owl_texture_type {
  OWL_TEXTURE_TYPE_2D,
  OWL_TEXTURE_TYPE_CUBE,
  OWL_TEXTURE_TYPE_COMPUTE
};

enum owl_texture_source { OWL_TEXTURE_SOURCE_FILE, OWL_TEXTURE_SOURCE_DATA };

enum owl_pixel_format {
  OWL_RGBA8_SRGB,
  OWL_RGBA8_UNORM,
  OWL_R8_UNORM,
  OWL_RGBA32_SFLOAT
};

struct owl_texture_desc {
  enum owl_texture_type type;
  enum owl_texture_source source;
  char const *path;
  void const *pixels;
  uint32_t width;
  uint32_t height;
  enum owl_pixel_format format;
};

struct owl_texture {
  uint32_t width;
  uint32_t height;
  uint32_t mipmaps;
  uint32_t layers;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkImageLayout layout;
  VkDescriptorSet descriptor_set;
};

OWLAPI int owl_texture_init(struct owl_renderer *r,
                            struct owl_texture_desc *desc,
                            struct owl_texture *texture);

OWLAPI void owl_texture_deinit(struct owl_renderer *r,
                               struct owl_texture *texture);

OWLAPI void owl_texture_change_layout(struct owl_texture *texture,
                                      VkCommandBuffer command_buffer,
                                      VkImageLayout layout);

uint32_t owl_texture_calculate_mipmaps(uint32_t w, uint32_t h);
OWL_END_DECLARATIONS

#endif

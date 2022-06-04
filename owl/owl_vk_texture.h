#ifndef OWL_VK_TEXTURE_H_
#define OWL_VK_TEXTURE_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_renderer;

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_texture_src {
  OWL_TEXTURE_SRC_FILE,
  OWL_TEXTURE_SRC_DATA
};

struct owl_vk_texture_desc {
  enum owl_texture_src src;

  char const *src_file_path;

  uint8_t *src_data;
  uint32_t src_data_width;
  uint32_t src_data_height;
  enum owl_pixel_format src_data_pixel_format;
};

struct owl_vk_texture {
  uint32_t width;
  uint32_t height;
  uint32_t mips;
  uint32_t layers;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkDescriptorSet descriptor_set;
  VkImageLayout layout;
};

owl_public owl_code
owl_vk_texture_init(struct owl_vk_texture *texture, struct owl_vk_renderer *vk,
                    struct owl_vk_texture_desc *desc);

owl_public void
owl_vk_texture_deinit(struct owl_vk_texture *texture,
                      struct owl_vk_renderer *vk);

owl_public VkFormat
owl_vk_pixel_format_as_vk_format(enum owl_pixel_format format);

owl_public uint64_t
owl_vk_pixel_format_size(enum owl_pixel_format format);

owl_public uint32_t
owl_vk_texture_calc_mips(int32_t w, int32_t h);

OWL_END_DECLS

#endif

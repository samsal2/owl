#ifndef OWL_VK_TEXTURE_H_
#define OWL_VK_TEXTURE_H_

#include "owl-definitions.h"

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

  owl_byte *src_data;
  owl_u32 src_data_width;
  owl_u32 src_data_height;
  enum owl_pixel_format src_data_pixel_format;
};

struct owl_vk_texture {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  owl_u32 layers;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView image_view;
  VkDescriptorSet descriptor_set;
  VkImageLayout layout;
};

owl_public enum owl_code
owl_vk_texture_init(struct owl_vk_texture *texture, struct owl_vk_renderer *vk,
                    struct owl_vk_texture_desc *desc);

owl_public void
owl_vk_texture_deinit(struct owl_vk_texture *texture,
                      struct owl_vk_renderer *vk);

owl_public VkFormat
owl_vk_pixel_format_as_vk_format(enum owl_pixel_format fmt);

owl_public owl_u64
owl_vk_pixel_format_size(enum owl_pixel_format fmt);

owl_public owl_u32
owl_vk_texture_calc_mips(owl_i32 w, owl_i32 h);

OWL_END_DECLS

#endif

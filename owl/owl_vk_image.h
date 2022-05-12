#ifndef OWL_VK_IMAGE_H_
#define OWL_VK_IMAGE_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_stage_heap;
struct owl_vk_pipeline_manager;

enum owl_sampler_mip_mode {
  OWL_SAMPLER_MIP_MODE_NEAREST,
  OWL_SAMPLER_MIP_MODE_LINEAR
};

enum owl_sampler_filter {
  OWL_SAMPLER_FILTER_NEAREST,
  OWL_SAMPLER_FILTER_LINEAR
};

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_sampler_address_mode {
  OWL_SAMPLER_ADDRESS_MODE_REPEAT,
  OWL_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
  OWL_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  OWL_SAMPLER_ADDRESS_MODEL_CLAMP_TO_BORDER
};

enum owl_vk_image_src_type {
  OWL_VK_IMAGE_SRC_TYPE_FILE,
  OWL_VK_IMAGE_SRC_TYPE_DATA
};

struct owl_vk_image_desc {
  enum owl_vk_image_src_type    src_type;
  char const                   *src_path;
  owl_byte const               *src_data;
  owl_i32                       src_data_width;
  owl_i32                       src_data_height;
  enum owl_pixel_format         src_data_pixel_format;
  owl_b32                       use_default_sampler;
  enum owl_sampler_mip_mode     sampler_mip_mode;
  enum owl_sampler_filter       sampler_min_filter;
  enum owl_sampler_filter       sampler_mag_filter;
  enum owl_sampler_address_mode sampler_wrap_u;
  enum owl_sampler_address_mode sampler_wrap_v;
  enum owl_sampler_address_mode sampler_wrap_w;
};

struct owl_vk_image {
  owl_i32         width;
  owl_i32         height;
  owl_u32         mips;
  VkImage         vk_image;
  VkDeviceMemory  vk_memory;
  VkImageView     vk_image_view;
  VkDescriptorSet vk_set;
  VkSampler       vk_sampler;
};

owl_public enum owl_code
owl_vk_image_init (struct owl_vk_image            *img,
                   struct owl_vk_context const    *ctx,
                   struct owl_vk_stage_heap       *heap,
                   struct owl_vk_image_desc const *desc);

owl_public void
owl_vk_image_deinit (struct owl_vk_image         *img,
                     struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

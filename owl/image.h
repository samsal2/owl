#ifndef OWL_IMAGE_H_
#define OWL_IMAGE_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_renderer;

struct owl_image {
  int slot;
};

enum owl_sampler_filter {
  OWL_SAMPLER_FILTER_NEAREST,
  OWL_SAMPLER_FILTER_LINEAR
};

enum owl_sampler_mip_mode {
  OWL_SAMPLER_MIP_MODE_NEAREST,
  OWL_SAMPLER_MIP_MODE_LINEAR
};

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_sampler_addr_mode {
  OWL_SAMPLER_ADDR_MODE_REPEAT,
  OWL_SAMPLER_ADDR_MODE_MIRRORED_REPEAT,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER
};

enum owl_image_init_info_source_type {
  OWL_IMAGE_INIT_INFO_SOURCE_TYPE_PATH,
  OWL_IMAGE_INIT_INFO_SOURCE_TYPE_DATA
};

enum owl_image_init_info_sampler_type {
  OWL_IMAGE_INIT_INFO_SAMPLER_TYPE_DEFAULT,
  OWL_IMAGE_INIT_INFO_SAMPLER_TYPE_SPECIFY
};

struct owl_image_init_info_source_path {
  char const *path;
};

struct owl_image_init_info_source_data {
  owl_byte const *data;
  int width;
  int height;
  enum owl_pixel_format format;
};

union owl_image_init_info_source_storage {
  struct owl_image_init_info_source_path as_path;
  struct owl_image_init_info_source_data as_data;
};

struct owl_image_init_info_sampler_specify {
  enum owl_sampler_mip_mode mip_mode;
  enum owl_sampler_filter min_filter;
  enum owl_sampler_filter mag_filter;
  enum owl_sampler_addr_mode wrap_u;
  enum owl_sampler_addr_mode wrap_v;
  enum owl_sampler_addr_mode wrap_w;
};

union owl_image_init_info_sampler_storage {
  struct owl_image_init_info_sampler_specify as_specify;
};

struct owl_image_init_info {
  enum owl_image_init_info_source_type source_type;
  union owl_image_init_info_source_storage source_storage;

  enum owl_image_init_info_sampler_type sampler_type;
  union owl_image_init_info_sampler_storage sampler_storage;
};

struct owl_image_info {
  owl_byte *data;
  int width;
  int height;
  enum owl_pixel_format format;
};

enum owl_code owl_image_load_info(char const *path, struct owl_image_info *ii);

void owl_image_free_info(struct owl_image_info *ii);

enum owl_code owl_image_init(struct owl_renderer *r,
                             struct owl_image_init_info const *iii,
                             struct owl_image *i);

void owl_image_deinit(struct owl_renderer *r, struct owl_image *i);

struct owl_vk_image_transition_info {
  owl_u32 mips;
  owl_u32 layers;
  VkImageLayout from;
  VkImageLayout to;
  VkImage image;
  VkCommandBuffer command_buffer;
};

enum owl_code
owl_vk_image_transition(struct owl_vk_image_transition_info const *iti);

struct owl_vk_image_mip_info {
  owl_i32 width;
  owl_i32 height;
  owl_u32 mips;
  VkImage image;
  VkCommandBuffer command_buffer;
};

enum owl_code
owl_vk_image_generate_mips(struct owl_vk_image_mip_info const *imi);

#endif

#ifndef OWL_TEXTURE_H_
#define OWL_TEXTURE_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_renderer;
struct owl_dynamic_buffer_reference;
struct owl_model_texture_data;

struct owl_texture {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  owl_u32 layer_count;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkDescriptorSet set;
  VkSampler sampler;
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

struct owl_texture_init_info {
  int width;
  int height;
  enum owl_pixel_format format;
  enum owl_sampler_mip_mode mip_mode;
  enum owl_sampler_filter min_filter;
  enum owl_sampler_filter mag_filter;
  enum owl_sampler_addr_mode wrap_u;
  enum owl_sampler_addr_mode wrap_v;
  enum owl_sampler_addr_mode wrap_w;
};

owl_u32 owl_calc_mips(owl_u32 width, owl_u32 height);

owl_byte *owl_texture_data_from_file(char const *path,
                                     struct owl_texture_init_info *info);

void owl_texture_free_data_from_file(owl_byte *data);

enum owl_code owl_texture_init_from_reference(
    struct owl_renderer *r, struct owl_texture_init_info const *info,
    struct owl_dynamic_buffer_reference const *ref, struct owl_texture *tex);

enum owl_code owl_texture_init_from_file(struct owl_renderer *r,
                                         struct owl_texture_init_info *info,
                                         char const *path,
                                         struct owl_texture *tex);

enum owl_code
owl_texture_init_from_data(struct owl_renderer *r,
                           struct owl_texture_init_info const *info,
                           owl_byte const *data, struct owl_texture *tex);

void owl_texture_deinit(struct owl_renderer const *r, struct owl_texture *tex);

enum owl_code owl_texture_create_from_file(struct owl_renderer *r,
                                           struct owl_texture_init_info *info,
                                           char const *path,
                                           struct owl_texture **tex);

enum owl_code
owl_texture_create_from_data(struct owl_renderer *r,
                             struct owl_texture_init_info const *info,
                             owl_byte const *data, struct owl_texture **tex);

void owl_texture_destroy(struct owl_renderer const *r, struct owl_texture *tex);

struct owl_vk_image_transition_info {
  owl_u32 mips;
  owl_u32 layers;
  VkImageLayout from;
  VkImageLayout to;
  VkImage image;
};

enum owl_code
owl_vk_image_transition(VkCommandBuffer cmd,
                        struct owl_vk_image_transition_info const *info);

struct owl_vk_image_mip_info {
  owl_i32 width;
  owl_i32 height;
  owl_u32 mips;
  VkImage image;
};

enum owl_code
owl_vk_image_generate_mips(VkCommandBuffer cmd,
                           struct owl_vk_image_mip_info const *info);

VkFormat owl_as_vk_format_(enum owl_pixel_format format);

VkDeviceSize
owl_texture_init_info_required_size_(struct owl_texture_init_info const *info);

#endif

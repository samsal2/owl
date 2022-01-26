#ifndef OWL_TEXTURE_H_
#define OWL_TEXTURE_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_vk_renderer;
struct owl_dyn_buf_ref;

struct owl_texture {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  owl_u32 layer_count;
  VkImage image;
  VkDeviceMemory mem;
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

struct owl_texture_desc {
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

enum owl_code owl_init_vk_texture_from_ref(struct owl_vk_renderer *r,
                                           struct owl_texture_desc const *desc,
                                           struct owl_dyn_buf_ref const *ref,
                                           struct owl_texture *tex);

enum owl_code owl_init_vk_texture_from_file(struct owl_vk_renderer *r,
                                            struct owl_texture_desc *desc,
                                            char const *path,
                                            struct owl_texture *tex);

enum owl_code owl_init_vk_texture_from_data(struct owl_vk_renderer *r,
                                            struct owl_texture_desc const *desc,
                                            owl_byte const *data,
                                            struct owl_texture *tex);

void owl_deinit_vk_texture(struct owl_vk_renderer const *r,
                           struct owl_texture *tex);

enum owl_code owl_create_texture_from_file(struct owl_vk_renderer *r,
                                           struct owl_texture_desc *desc,
                                           char const *path,
                                           struct owl_texture **tex);

enum owl_code owl_create_texture_from_data(struct owl_vk_renderer *r,
                                           struct owl_texture_desc const *desc,
                                           owl_byte const *data,
                                           struct owl_texture **tex);

void owl_destroy_texture(struct owl_vk_renderer const *r,
                         struct owl_texture *tex);

enum owl_code owl_alloc_and_record_cmd_buf(struct owl_vk_renderer const *r,
                                           VkCommandBuffer *cmd);

enum owl_code owl_free_and_submit_cmd_buf(struct owl_vk_renderer const *r,
                                          VkCommandBuffer cmd);

struct owl_vk_image_trans_desc {
  owl_u32 mips;
  VkImageLayout from;
  VkImageLayout to;
  VkImage image;
};

enum owl_code
owl_transition_vk_image_layout(VkCommandBuffer cmd,
                               struct owl_vk_image_trans_desc const *desc);

struct owl_vk_mip_gen_desc {
  owl_i32 width;
  owl_i32 height;
  owl_u32 mips;
  VkImage image;
};

enum owl_code
owl_generate_vk_image_mips(VkCommandBuffer cmd,
                           struct owl_vk_mip_gen_desc const *desc);

#endif

#ifndef OWL_VK_TEXTURE_H_
#define OWL_VK_TEXTURE_H_

#include <owl/fwd.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

struct owl_tmp_submit_mem_ref;

struct owl_vk_texture {
  OwlU32 width;
  OwlU32 height;
  OwlU32 mips;
  VkImage img;
  VkImageView view;
  VkDeviceMemory mem;
  VkDescriptorSet set;
  VkSampler sampler;
};

enum owl_code owl_init_vk_texture(struct owl_vk_renderer *renderer,
                                  struct owl_texture_attr const *attr,
                                  OwlByte const *data,
                                  struct owl_vk_texture *texture);

enum owl_code
owl_init_vk_texture_with_ref(struct owl_vk_renderer *renderer,
                             struct owl_texture_attr const *attr,
                             struct owl_tmp_submit_mem_ref const *ref,
                             struct owl_vk_texture *texture);

void owl_deinit_vk_texture(struct owl_vk_renderer const *renderer,
                           struct owl_vk_texture *texture);

#endif

#ifndef OWL_VK_FRAME_H_
#define OWL_VK_FRAME_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_renderer;

struct owl_vk_frame_allocation {
  uint32_t offset32;
  VkDeviceSize offset;
  VkBuffer buffer;
  VkDescriptorSet pvm_descriptor_set;
  VkDescriptorSet model1_descriptor_set;
  VkDescriptorSet model2_descriptor_set;
};

owl_public owl_code
owl_vk_frame_begin(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_frame_end(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_frame_reserve(struct owl_vk_renderer *vk, uint64_t size);

owl_public void *
owl_vk_frame_alloc(struct owl_vk_renderer *vk, uint64_t size,
                   struct owl_vk_frame_allocation *alloc);

OWL_END_DECLS

#endif

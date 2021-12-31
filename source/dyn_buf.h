#ifndef OWL_SOURCE_DYN_BUF_H_
#define OWL_SOURCE_DYN_BUF_H_

#include "../include/owl/types.h"

#include <vulkan/vulkan.h>

struct owl_vk_renderer;

struct owl_dyn_buf_ref {
  OwlU32 offset32; /* for uniform usage */
  VkDeviceSize offset;
  VkBuffer buf;
  VkDescriptorSet pvm_set;
};

void *owl_dyn_buf_alloc(struct owl_vk_renderer *renderer, VkDeviceSize size,
                        struct owl_dyn_buf_ref *ref);

#endif

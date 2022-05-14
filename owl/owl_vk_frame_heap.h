#ifndef OWL_VK_FRAME_HEAP_H_
#define OWL_VK_FRAME_HEAP_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;

struct owl_vk_frame_heap {
  void *data;
  VkDeviceSize offset;
  VkDeviceSize size;
  VkDeviceSize alignment;
  VkDeviceMemory vk_memory;
  VkBuffer vk_buffer;
  VkDescriptorSet vk_pvm_ubo_set;
  VkDescriptorSet vk_model_ubo1_set;
  VkDescriptorSet vk_model_ubo2_set;
};

owl_public enum owl_code
owl_vk_frame_heap_init (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx, owl_u64 sz);

owl_public void
owl_vk_frame_heap_deinit (struct owl_vk_frame_heap *heap,
                          struct owl_vk_context const *ctx);

owl_public void
owl_vk_frame_heap_unmap (struct owl_vk_frame_heap *heap,
                         struct owl_vk_context const *ctx);

struct owl_vk_frame_allocation {
  owl_u32 offset32;
  VkDeviceSize offset;
  VkBuffer vk_buffer;
  VkDescriptorSet vk_pvm_ubo_set;
  VkDescriptorSet vk_model_ubo1_set;
  VkDescriptorSet vk_model_ubo2_set;
};

owl_public void *
owl_vk_frame_heap_unsafe_allocate (struct owl_vk_frame_heap *heap,
                                   struct owl_vk_context const *ctx,
                                   owl_u64 sz,
                                   struct owl_vk_frame_allocation *allocation);

owl_public void
owl_vk_frame_heap_free (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx);

owl_public owl_b32
owl_vk_frame_heap_has_enough_space (struct owl_vk_frame_heap const *heap,
                                    owl_u64 sz);

owl_public owl_u64
owl_vk_frame_heap_offset (struct owl_vk_frame_heap const *heap);

owl_public void
owl_vk_frame_heap_unsafe_copy (struct owl_vk_frame_heap *dst,
                               struct owl_vk_frame_heap const *src);

OWL_END_DECLS

#endif

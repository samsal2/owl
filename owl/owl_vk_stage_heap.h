#ifndef OWL_VK_STAGE_HEAP_H_
#define OWL_VK_STAGE_HEAP_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;

struct owl_vk_stage_heap {
  owl_b32 in_use;
  owl_u64 size;
  void *data;
  VkBuffer vk_buffer;
  VkDeviceMemory vk_memory;
};

struct owl_vk_stage_heap_allocation {
  VkBuffer vk_buffer;
};

owl_public enum owl_code
owl_vk_stage_heap_init (struct owl_vk_stage_heap *heap,
                        struct owl_vk_context const *ctx, owl_u64 sz);

owl_public void
owl_vk_stage_heap_deinit (struct owl_vk_stage_heap *heap,
                          struct owl_vk_context const *ctx);

owl_public owl_b32
owl_vk_stage_heap_enough_space (struct owl_vk_stage_heap *heap, owl_u64 sz);

owl_public void *
owl_vk_stage_heap_allocate (struct owl_vk_stage_heap *heap,
                            struct owl_vk_context const *ctx, owl_u64 sz,
                            struct owl_vk_stage_heap_allocation *allocation);

owl_public void
owl_vk_stage_heap_free (struct owl_vk_stage_heap *heap,
                        struct owl_vk_context const *ctx, void *p);

OWL_END_DECLS

#endif

#ifndef OWL_VK_GARBAGE_H_
#define OWL_VK_GARBAGE_H_

#include "owl_definitions.h"
#include "owl_vk_frame_heap.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_frame;

#define OWL_VK_GARBAGE_MAX_HEAP_COUNT 16
struct owl_vk_garbage {
  owl_i32 heap_count;
  struct owl_vk_frame_heap heaps[OWL_VK_GARBAGE_MAX_HEAP_COUNT];
};

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage *garbage,
                     struct owl_vk_context const *ctx);

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage *garbage,
                       struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame);

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame);

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage *garbage,
                      struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

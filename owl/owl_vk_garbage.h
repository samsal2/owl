#ifndef OWL_VK_GARBAGE_H_
#define OWL_VK_GARBAGE_H_

#include "owl_definitions.h"
#include "owl_vk_frame_heap.h"
#include "owl_vk_frame_sync.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_frame;

struct owl_vk_frame_garbage {
  struct owl_vk_frame_heap heap;
};

owl_public void
owl_vk_frame_garbage_init (struct owl_vk_frame_garbage *garbage,
                           struct owl_vk_frame const   *frame);

owl_public void
owl_vk_frame_garbage_deinit (struct owl_vk_frame_garbage *garbage,
                             struct owl_vk_context const *ctx);

#define OWL_VK_GARBAGE_MAX_FRAME_COUNT 16
struct owl_vk_garbage {
  owl_i32                     frame_count;
  struct owl_vk_frame_garbage frames[OWL_VK_GARBAGE_MAX_FRAME_COUNT];
};

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage       *garbage,
                     struct owl_vk_context const *ctx);

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage       *garbage,
                       struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame   *frame);

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame   *frame);

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage       *garbage,
                      struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

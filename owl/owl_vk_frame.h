#ifndef OWL_VK_FRAME_H_
#define OWL_VK_FRAME_H_

#include "owl_definitions.h"
#include "owl_vk_frame_heap.h"
#include "owl_vk_frame_sync.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_garbage;
struct owl_vk_swapchain;

struct owl_vk_frame {
  VkCommandPool vk_command_pool;
  VkCommandBuffer vk_command_buffer;
  struct owl_vk_frame_heap heap;
  struct owl_vk_frame_sync sync;
};

owl_public enum owl_code
owl_vk_frame_init (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx, owl_u64 size);

owl_public void
owl_vk_frame_deinit (struct owl_vk_frame *frame,
                     struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_frame_wait (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_frame_prepare (struct owl_vk_frame *frame,
                      struct owl_vk_context const *ctx);

owl_public void *
owl_vk_frame_allocate (struct owl_vk_frame *frame,
                       struct owl_vk_context const *ctx,
                       struct owl_vk_garbage *garbage, owl_u64 size,
                       struct owl_vk_frame_allocation *alloc);

owl_public void
owl_vk_frame_free (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_frame_begin (struct owl_vk_frame *frame,
                    struct owl_vk_context const *ctx,
                    struct owl_vk_swapchain *sc);

owl_public enum owl_code
owl_vk_frame_end (struct owl_vk_frame *frame, struct owl_vk_context const *ctx,
                  struct owl_vk_swapchain *sc);

owl_public enum owl_code
owl_vk_frame_resync (struct owl_vk_frame *frame,
                     struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

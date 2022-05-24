#ifndef OWL_VK_FRAME_SYNC_H_
#define OWL_VK_FRAME_SYNC_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;

struct owl_vk_frame_sync {
  VkFence vk_in_flight;
  VkSemaphore vk_render_done;
  VkSemaphore vk_image_available;
};

owl_public enum owl_code
owl_vk_frame_sync_init (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context const *ctx);

owl_public void
owl_vk_frame_sync_deinit (struct owl_vk_frame_sync *sync,
                          struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_frame_sync_wait (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_frame_sync_reset (struct owl_vk_frame_sync *sync,
                         struct owl_vk_context const *ctx);

owl_public void
owl_vk_frame_sync_unsafe_copy (struct owl_vk_frame_sync *dst,
                               struct owl_vk_frame_sync const *src);

OWL_END_DECLS

#endif

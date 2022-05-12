#ifndef OWL_VK_SWAPCHAIN_H_
#define OWL_VK_SWAPCHAIN_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_attachment;
struct owl_vk_frame_sync;

#define OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT 8
struct owl_vk_swapchain {
  VkSwapchainKHR vk_swapchain;

  VkExtent2D   size;
  VkClearValue clear_values[2];

  owl_u32       image;
  owl_u32       vk_image_count;
  VkImage       vk_images[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
  VkImageView   vk_image_views[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
  VkFramebuffer vk_framebuffers[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
};

owl_public enum owl_code
owl_vk_swapchain_init (struct owl_vk_swapchain        *swapchain,
                       struct owl_vk_context const    *ctx,
                       struct owl_vk_attachment const *color,
                       struct owl_vk_attachment const *depth_stencil);

owl_public void
owl_vk_swapchain_deinit (struct owl_vk_swapchain     *swapchain,
                         struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_swapchain_acquire_next_image (struct owl_vk_swapchain        *swapchain,
                                     struct owl_vk_context const    *ctx,
                                     struct owl_vk_frame_sync const *sync);

owl_public enum owl_code
owl_vk_swapchain_present (struct owl_vk_swapchain        *swapchain,
                          struct owl_vk_context const    *ctx,
                          struct owl_vk_frame_sync const *sync);

OWL_END_DECLS

#endif

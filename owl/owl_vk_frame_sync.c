#include "owl_vk_frame_sync.h"

#include "owl_vk_context.h"

owl_public enum owl_code
owl_vk_frame_sync_init (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context const *ctx) {
  VkFenceCreateInfo fence_info;
  VkSemaphoreCreateInfo semaphore_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = NULL;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vk_result = vkCreateFence (ctx->vk_device, &fence_info, NULL,
                             &sync->vk_in_flight_fence);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = NULL;
  semaphore_info.flags = 0;

  vk_result = vkCreateSemaphore (ctx->vk_device, &semaphore_info, NULL,
                                 &sync->vk_render_done_semaphore);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_in_flight_fence_deinit;
  }

  vk_result = vkCreateSemaphore (ctx->vk_device, &semaphore_info, NULL,
                                 &sync->vk_image_available_semaphore);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_render_done_semaphore_deinit;
  }

  goto out;

out_error_render_done_semaphore_deinit:
  vkDestroySemaphore (ctx->vk_device, sync->vk_render_done_semaphore, NULL);

out_error_in_flight_fence_deinit:
  vkDestroyFence (ctx->vk_device, sync->vk_in_flight_fence, NULL);

out:
  return code;
}

owl_public void
owl_vk_frame_sync_deinit (struct owl_vk_frame_sync *sync,
                          struct owl_vk_context const *ctx) {
  vkDestroySemaphore (ctx->vk_device, sync->vk_image_available_semaphore,
                      NULL);
  vkDestroySemaphore (ctx->vk_device, sync->vk_render_done_semaphore, NULL);
  vkDestroyFence (ctx->vk_device, sync->vk_in_flight_fence, NULL);
}

owl_public enum owl_code
owl_vk_frame_sync_wait (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context const *ctx) {
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkWaitForFences (ctx->vk_device, 1, &sync->vk_in_flight_fence,
                               VK_TRUE, (owl_u64)-1);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_public enum owl_code
owl_vk_frame_sync_reset (struct owl_vk_frame_sync *sync,
                         struct owl_vk_context const *ctx) {
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkResetFences (ctx->vk_device, 1, &sync->vk_in_flight_fence);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_public void
owl_vk_frame_sync_unsafe_copy (struct owl_vk_frame_sync *dst,
                               struct owl_vk_frame_sync const *src) {
  dst->vk_in_flight_fence = src->vk_in_flight_fence;
  dst->vk_render_done_semaphore = src->vk_render_done_semaphore;
  dst->vk_image_available_semaphore = src->vk_image_available_semaphore;
}

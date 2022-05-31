#include "owl_vk_frame.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_garbage.h"
#include "owl_vk_swapchain.h"

owl_private enum owl_code
owl_vk_frame_commands_init (struct owl_vk_frame *frame,
                            struct owl_vk_context const *ctx) {
  VkCommandPoolCreateInfo command_pool_info;
  VkCommandBufferAllocateInfo command_buffer_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.pNext = NULL;
  command_pool_info.flags = 0;
  command_pool_info.queueFamilyIndex = ctx->vk_graphics_queue_family;

  vk_result = vkCreateCommandPool (ctx->vk_device, &command_pool_info, NULL,
                                   &frame->vk_command_pool);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.commandPool = frame->vk_command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;

  vk_result = vkAllocateCommandBuffers (ctx->vk_device, &command_buffer_info,
                                        &frame->vk_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_command_pool_deinit;
  }

  goto out;

error_command_pool_deinit:
  vkDestroyCommandPool (ctx->vk_device, frame->vk_command_pool, NULL);

out:
  return code;
}

owl_private void
owl_vk_frame_commands_deinit (struct owl_vk_frame *frame,
                              struct owl_vk_context const *ctx) {
  vkFreeCommandBuffers (ctx->vk_device, frame->vk_command_pool, 1,
                        &frame->vk_command_buffer);
  vkDestroyCommandPool (ctx->vk_device, frame->vk_command_pool, NULL);
}

owl_public enum owl_code
owl_vk_frame_init (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx, owl_u64 size) {
  enum owl_code code;

  code = owl_vk_frame_commands_init (frame, ctx);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_frame_heap_init (&frame->heap, ctx, size);
  if (OWL_SUCCESS != code)
    goto error_commads_deinit;

  code = owl_vk_frame_sync_init (&frame->sync, ctx);
  if (OWL_SUCCESS != code)
    goto error_frame_heap_deinit;

  goto out;

error_frame_heap_deinit:
  owl_vk_frame_heap_deinit (&frame->heap, ctx);

error_commads_deinit:
  owl_vk_frame_commands_deinit (frame, ctx);

out:
  return code;
}

owl_private enum owl_code
owl_vk_frame_commands_reset (struct owl_vk_frame *frame,
                             struct owl_vk_context const *ctx) {
  VkResult vk_result;

  vk_result = vkResetCommandPool (ctx->vk_device, frame->vk_command_pool, 0);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_public void
owl_vk_frame_deinit (struct owl_vk_frame *frame,
                     struct owl_vk_context const *ctx) {
  owl_vk_frame_sync_deinit (&frame->sync, ctx);
  owl_vk_frame_heap_deinit (&frame->heap, ctx);
  owl_vk_frame_commands_deinit (frame, ctx);
}

owl_public enum owl_code
owl_vk_frame_wait (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx) {
  return owl_vk_frame_sync_wait (&frame->sync, ctx);
}

owl_public enum owl_code
owl_vk_frame_prepare (struct owl_vk_frame *frame,
                      struct owl_vk_context const *ctx) {
  enum owl_code code;

  code = owl_vk_frame_wait (frame, ctx);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_frame_sync_reset (&frame->sync, ctx);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_frame_commands_reset (frame, ctx);
  if (OWL_SUCCESS != code)
    return code;

  return OWL_SUCCESS;
}

owl_private owl_u64
owl_vk_frame_calc_new_size (struct owl_vk_frame *frame, owl_u64 request) {
  return owl_alignu2 (frame->heap.offset + request, frame->heap.alignment) * 2;
}

owl_public enum owl_code
owl_vk_frame_reserve (struct owl_vk_frame *frame,
                      struct owl_vk_context const *ctx,
                      struct owl_vk_garbage *garbage, owl_u64 size) {
  owl_u64 new_size;

  enum owl_code code = OWL_SUCCESS;

  if (owl_vk_frame_heap_has_enough_space (&frame->heap, size))
    goto out;

  code = owl_vk_garbage_add_frame (garbage, frame);
  if (OWL_SUCCESS != code)
    goto out;

  new_size = owl_vk_frame_calc_new_size (frame, size);

  code = owl_vk_frame_heap_init (&frame->heap, ctx, new_size);
  if (OWL_SUCCESS != code)
    goto error_pop_frame;

  goto out;

error_pop_frame:
  owl_vk_garbage_pop_frame (garbage, frame);

out:
  return code;
}

owl_public void *
owl_vk_frame_allocate (struct owl_vk_frame *frame,
                       struct owl_vk_context const *ctx,
                       struct owl_vk_garbage *garbage, owl_u64 size,
                       struct owl_vk_frame_allocation *alloc) {
  enum owl_code code;

  code = owl_vk_frame_reserve (frame, ctx, garbage, size);
  if (OWL_SUCCESS != code)
    return NULL;

  return owl_vk_frame_heap_unsafe_allocate (&frame->heap, ctx, size, alloc);
}

owl_public void
owl_vk_frame_free (struct owl_vk_frame *frame,
                   struct owl_vk_context const *ctx) {
  owl_vk_frame_heap_free (&frame->heap, ctx);
}

owl_private enum owl_code
owl_vk_frame_begin_recording (struct owl_vk_frame *frame,
                              struct owl_vk_context const *ctx,
                              struct owl_vk_swapchain *sc) {
  VkCommandBufferBeginInfo command_buffer_info;
  VkRenderPassBeginInfo render_pass_info;

  VkResult vk_result = VK_SUCCESS;

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_info.pInheritanceInfo = NULL;

  vk_result =
      vkBeginCommandBuffer (frame->vk_command_buffer, &command_buffer_info);
  if (OWL_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.pNext = NULL;
  render_pass_info.renderPass = ctx->vk_main_render_pass;
  render_pass_info.framebuffer = sc->vk_framebuffers[sc->image];
  render_pass_info.renderArea.offset.x = 0;
  render_pass_info.renderArea.offset.y = 0;
  render_pass_info.renderArea.extent = sc->size;
  render_pass_info.clearValueCount = owl_array_size (sc->clear_values);
  render_pass_info.pClearValues = sc->clear_values;

  vkCmdBeginRenderPass (frame->vk_command_buffer, &render_pass_info,
                        VK_SUBPASS_CONTENTS_INLINE);

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_frame_end_recording (struct owl_vk_frame *frame,
                            struct owl_vk_context const *ctx) {
  VkResult vk_result = VK_SUCCESS;

  owl_unused (ctx);

  vkCmdEndRenderPass (frame->vk_command_buffer);

  vk_result = vkEndCommandBuffer (frame->vk_command_buffer);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_frame_submit (struct owl_vk_frame *frame,
                     struct owl_vk_context const *ctx) {
  VkSubmitInfo info;

  VkResult vk_result = VK_SUCCESS;
  VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = NULL;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &frame->sync.vk_image_available;
  info.signalSemaphoreCount = 1;
  info.pSignalSemaphores = &frame->sync.vk_render_done;
  info.pWaitDstStageMask = &stage;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &frame->vk_command_buffer;

  vk_result = vkQueueSubmit (ctx->vk_graphics_queue, 1, &info,
                             frame->sync.vk_in_flight);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_frame_begin (struct owl_vk_frame *frame,
                    struct owl_vk_context const *ctx,
                    struct owl_vk_swapchain *sc) {
  enum owl_code code;

  /* free the previous allocations */
  owl_vk_frame_free (frame, ctx);

  code = owl_vk_swapchain_acquire_next_image (sc, ctx, &frame->sync);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_frame_prepare (frame, ctx);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_frame_begin_recording (frame, ctx, sc);
  if (OWL_SUCCESS != code)
    return code;

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_frame_end (struct owl_vk_frame *frame, struct owl_vk_context const *ctx,
                  struct owl_vk_swapchain *sc) {
  enum owl_code code;

  code = owl_vk_frame_end_recording (frame, ctx);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_frame_submit (frame, ctx);
  if (OWL_SUCCESS != code)
    return code;

  code = owl_vk_swapchain_present (sc, ctx, &frame->sync);
  if (OWL_SUCCESS != code)
    return code;

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_frame_resync (struct owl_vk_frame *frame,
                     struct owl_vk_context const *ctx) {
  owl_vk_frame_sync_deinit (&frame->sync, ctx);
  return owl_vk_frame_sync_init (&frame->sync, ctx);
}

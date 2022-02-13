#include "frame.h"

#include "internal.h"
#include "renderer.h"
#include "types.h"

#define OWL_VK_TIMEOUT (owl_u64) - 1

OWL_INTERNAL enum owl_code owl_acquire_next_image_(struct owl_vk_renderer *r) {
  VkResult const result =
      vkAcquireNextImageKHR(r->device, r->swapchain, OWL_VK_TIMEOUT,
                            r->image_available_semaphores[r->active],
                            VK_NULL_HANDLE, &r->swapchain_active_image);

  if (VK_ERROR_OUT_OF_DATE_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  if (VK_SUBOPTIMAL_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  if (VK_ERROR_SURFACE_LOST_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_prepare_frame_(struct owl_vk_renderer *r) {
  OWL_VK_CHECK(vkWaitForFences(r->device, 1, &r->in_flight_fences[r->active],
                               VK_TRUE, OWL_VK_TIMEOUT));

  OWL_VK_CHECK(vkResetFences(r->device, 1, &r->in_flight_fences[r->active]));

  OWL_VK_CHECK(vkResetCommandPool(r->device, r->frame_cmd_pools[r->active], 0));
}

OWL_INTERNAL void owl_start_recording_(struct owl_vk_renderer *r) {
  {
    VkCommandBufferBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = NULL;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin.pInheritanceInfo = NULL;

    OWL_VK_CHECK(vkBeginCommandBuffer(r->frame_cmd_buffers[r->active], &begin));
  }

  {
    VkRenderPassBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.pNext = NULL;
    begin.renderPass = r->main_render_pass;
    begin.framebuffer = r->framebuffers[r->swapchain_active_image];
    begin.renderArea.offset.x = 0;
    begin.renderArea.offset.y = 0;
    begin.renderArea.extent = r->swapchain_extent;
    begin.clearValueCount = OWL_ARRAY_SIZE(r->swapchain_clear_values);
    begin.pClearValues = r->swapchain_clear_values;

    vkCmdBeginRenderPass(r->frame_cmd_buffers[r->active], &begin,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
}

enum owl_code owl_frame_begin(struct owl_vk_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_acquire_next_image_(r)))
    goto end;

  owl_prepare_frame_(r);
  owl_start_recording_(r);

end:
  return code;
}
#undef OWL_VK_TIMEOUT

OWL_INTERNAL void owl_end_recording_(struct owl_vk_renderer *r) {
  vkCmdEndRenderPass(r->frame_cmd_buffers[r->active]);
  OWL_VK_CHECK(vkEndCommandBuffer(r->frame_cmd_buffers[r->active]));
}

OWL_INTERNAL void owl_submit_graphics_(struct owl_vk_renderer *r) {
#define OWL_VK_WAIT_STAGE VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  VkSubmitInfo submit;
  VkPipelineStageFlags const stage = OWL_VK_WAIT_STAGE;

  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = NULL;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &r->image_available_semaphores[r->active];
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &r->render_done_semaphores[r->active];
  submit.pWaitDstStageMask = &stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &r->frame_cmd_buffers[r->active];

  OWL_VK_CHECK(vkQueueSubmit(r->graphics_queue, 1, &submit,
                             r->in_flight_fences[r->active]));
#undef OWL_VK_WAIT_STAGE
}

OWL_INTERNAL enum owl_code owl_present_swapchain_(struct owl_vk_renderer *r) {
  VkResult result;
  VkPresentInfoKHR present;

  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = NULL;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &r->render_done_semaphores[r->active];
  present.swapchainCount = 1;
  present.pSwapchains = &r->swapchain;
  present.pImageIndices = &r->swapchain_active_image;
  present.pResults = NULL;

  result = vkQueuePresentKHR(r->present_queue, &present);

  if (VK_ERROR_OUT_OF_DATE_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  if (VK_SUBOPTIMAL_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  if (VK_ERROR_SURFACE_LOST_KHR == result)
    return OWL_ERROR_OUTDATED_SWAPCHAIN;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_swap_active_(struct owl_vk_renderer *r) {
  if (OWL_VK_RENDERER_DYN_BUFFER_COUNT == ++r->active)
    r->active = 0;
}

enum owl_code owl_frame_end(struct owl_vk_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  owl_end_recording_(r);
  owl_submit_graphics_(r);

  if (OWL_SUCCESS != (code = owl_present_swapchain_(r)))
    goto end;

  owl_swap_active_(r);
  /* reset dyn buffer */
  owl_renderer_clear_dyn_offset(r);
  owl_renderer_clear_dyn_garbage(r);

end:
  return code;
}

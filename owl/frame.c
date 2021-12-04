#include <owl/frame.h>
#include <owl/internal.h>
#include <owl/renderer.inl>
#include <owl/types.h>
#include <owl/vkutil.h>

#define OWL_VK_TIMEOUT (OwlU64) - 1

enum owl_code owl_begin_frame(struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;
  int const active = renderer->cmd.active;

  {
    VkResult const result = vkAcquireNextImageKHR(
        renderer->device.logical, renderer->swapchain.handle,
        OWL_VK_TIMEOUT, renderer->sync.img_available[active],
        VK_NULL_HANDLE, &renderer->swapchain.current);

    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }

#if 1
    if (VK_SUBOPTIMAL_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }
#endif

    if (VK_ERROR_SURFACE_LOST_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }
  }

  {
    OWL_VK_CHECK(vkWaitForFences(renderer->device.logical, 1,
                                   &renderer->sync.in_flight[active], VK_TRUE,
                                   OWL_VK_TIMEOUT));

    OWL_VK_CHECK(vkResetFences(renderer->device.logical, 1,
                                 &renderer->sync.in_flight[active]));

    OWL_VK_CHECK(vkResetCommandPool(renderer->device.logical,
                                      renderer->cmd.pools[active], 0));
  }

  {
    VkCommandBufferBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = NULL;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin.pInheritanceInfo = NULL;

    OWL_VK_CHECK(vkBeginCommandBuffer(renderer->cmd.buffs[active], &begin));
  }

  {
    VkRenderPassBeginInfo begin;
    OwlU32 const image = renderer->swapchain.current;

    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.pNext = NULL;
    begin.renderPass = renderer->main_pass;
    begin.framebuffer = renderer->swapchain.framebuffers[image];
    begin.renderArea.offset.x = 0;
    begin.renderArea.offset.y = 0;
    begin.renderArea.extent = renderer->swapchain.extent;
    begin.clearValueCount = OWL_ARRAY_SIZE(renderer->swapchain.clear_vals);
    begin.pClearValues = renderer->swapchain.clear_vals;

    vkCmdBeginRenderPass(renderer->cmd.buffs[active], &begin,
                         VK_SUBPASS_CONTENTS_INLINE);
  }

end:
  return err;
}

#undef OWL_VK_TIMEOUT

#define OWL_WAIT_STAGE VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT

enum owl_code owl_end_frame(struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;
  int const active = renderer->cmd.active;

  {
    vkCmdEndRenderPass(renderer->cmd.buffs[active]);
    OWL_VK_CHECK(vkEndCommandBuffer(renderer->cmd.buffs[active]));
  }

  {
    VkSubmitInfo submit;
    VkPipelineStageFlags const stage = OWL_WAIT_STAGE;

    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = NULL;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &renderer->sync.img_available[active];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderer->sync.render_finished[active];
    submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &renderer->cmd.buffs[active];

    OWL_VK_CHECK(
        vkQueueSubmit(renderer->device.queues[OWL_VK_QUEUE_TYPE_GRAPHICS],
                      1, &submit, renderer->sync.in_flight[active]));
  }

  {
    VkResult result;
    VkPresentInfoKHR present;

    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderer->sync.render_finished[active];
    present.swapchainCount = 1;
    present.pSwapchains = &renderer->swapchain.handle;
    present.pImageIndices = &renderer->swapchain.current;
    present.pResults = NULL;

    result = vkQueuePresentKHR(
        renderer->device.queues[OWL_VK_QUEUE_TYPE_PRESENT], &present);

    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }

#if 1
    if (VK_SUBOPTIMAL_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }
#endif

    if (VK_ERROR_SURFACE_LOST_KHR == result) {
      err = OWL_ERROR_UNKNOWN;
      goto end;
    }
  }

  /* swap actives */
  {
    if (OWL_DYNAMIC_BUFFER_COUNT == ++renderer->cmd.active)
      renderer->cmd.active = 0;

    if (OWL_DYNAMIC_BUFFER_COUNT == ++renderer->dbl_buff.active)
      renderer->dbl_buff.active = 0;
  }

  /* reset stuff */
  {
    renderer->dbl_buff.offsets[renderer->dbl_buff.active] = 0;
    owl_clear_garbage(renderer);
  }

end:
  return err;
}

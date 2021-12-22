#include <owl/code.h>
#include <owl/frame.h>
#include <owl/internal.h>
#include <owl/renderer_internal.h>
#include <owl/types.h>
#include <owl/vkutil.h>

#define OWL_VK_TIMEOUT (OwlU64) - 1

enum owl_code owl_begin_frame(struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;
  OwlU32 const active = renderer->active_buf;

  {
    VkResult const result = vkAcquireNextImageKHR(
        renderer->device, renderer->swapchain, OWL_VK_TIMEOUT,
        renderer->img_available_sema[active], VK_NULL_HANDLE,
        &renderer->active_img);

    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }

#if 1
    if (VK_SUBOPTIMAL_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }
#endif

    if (VK_ERROR_SURFACE_LOST_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }
  }

  {
    OWL_VK_CHECK(vkWaitForFences(renderer->device, 1,
                                 &renderer->in_flight_fence[active], VK_TRUE,
                                 OWL_VK_TIMEOUT));

    OWL_VK_CHECK(vkResetFences(renderer->device, 1,
                               &renderer->in_flight_fence[active]));

    OWL_VK_CHECK(vkResetCommandPool(renderer->device,
                                    renderer->draw_cmd_pools[active], 0));
  }

  {
    VkCommandBufferBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = NULL;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin.pInheritanceInfo = NULL;

    OWL_VK_CHECK(
        vkBeginCommandBuffer(renderer->draw_cmd_bufs[active], &begin));
  }

  {
    VkRenderPassBeginInfo begin;
    OwlU32 const img = renderer->active_img;

    begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin.pNext = NULL;
    begin.renderPass = renderer->main_render_pass;
    begin.framebuffer = renderer->framebuffers[img];
    begin.renderArea.offset.x = 0;
    begin.renderArea.offset.y = 0;
    begin.renderArea.extent = renderer->swapchain_extent;
    begin.clearValueCount = OWL_ARRAY_SIZE(renderer->clear_vals);
    begin.pClearValues = renderer->clear_vals;

    vkCmdBeginRenderPass(renderer->draw_cmd_bufs[active], &begin,
                         VK_SUBPASS_CONTENTS_INLINE);
  }

end:
  return err;
}

#undef OWL_VK_TIMEOUT

#define OWL_WAIT_STAGE VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT

enum owl_code owl_end_frame(struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;
  OwlU32 const active = renderer->active_buf;

  {
    vkCmdEndRenderPass(renderer->draw_cmd_bufs[active]);
    OWL_VK_CHECK(vkEndCommandBuffer(renderer->draw_cmd_bufs[active]));
  }

  {
    VkSubmitInfo submit;
    VkPipelineStageFlags const stage = OWL_WAIT_STAGE;

    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = NULL;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &renderer->img_available_sema[active];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderer->renderer_finished_sema[active];
    submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &renderer->draw_cmd_bufs[active];

    OWL_VK_CHECK(vkQueueSubmit(renderer->graphics_queue, 1, &submit,
                               renderer->in_flight_fence[active]));
  }

  {
    VkResult result;
    VkPresentInfoKHR present;

    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderer->renderer_finished_sema[active];
    present.swapchainCount = 1;
    present.pSwapchains = &renderer->swapchain;
    present.pImageIndices = &renderer->active_img;
    present.pResults = NULL;

    result = vkQueuePresentKHR(renderer->present_queue, &present);

    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }

#if 1
    if (VK_SUBOPTIMAL_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }
#endif

    if (VK_ERROR_SURFACE_LOST_KHR == result) {
      err = OWL_ERROR_OUTDATED_SWAPCHAIN;
      goto end;
    }
  }

  /* swap actives */
  {
    if (OWL_DYN_BUF_COUNT == ++renderer->active_buf)
      renderer->active_buf = 0;
  }

  /* reset stuff */
  {
    renderer->dyn_offsets[renderer->active_buf] = 0;
    owl_clear_garbage(renderer);
  }

end:
  return err;
}

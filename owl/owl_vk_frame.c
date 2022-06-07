#include "owl_vk_frame.h"

#include "owl_definitions.h"
#include "owl_internal.h"
#include "owl_vk_renderer.h"
#include "owl_vk_types.h"

owl_private void
owl_vk_frame_collect_garbage(struct owl_vk_renderer *vk)
{
  uint32_t i;

  owl_vector(VkBuffer) buffers = vk->garbage_buffers[vk->frame];
  owl_vector(VkDeviceMemory) memories = vk->garbage_memories[vk->frame];
  owl_vector(VkDescriptorSet) sets = vk->garbage_sets[vk->frame];

  for (i = 0; i < owl_vector_size(sets); ++i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1, &sets[i]);

  owl_vector_clear(sets);

  for (i = 0; i < owl_vector_size(memories); ++i)
    vkFreeMemory(vk->device, memories[i], NULL);

  owl_vector_clear(memories);

  for (i = 0; i < owl_vector_size(buffers); ++i)
    vkDestroyBuffer(vk->device, buffers[i], NULL);

  owl_vector_clear(buffers);
}

owl_private owl_code
owl_vk_frame_push_garbage(struct owl_vk_renderer *vk)
{
  owl_code code;
  int i;

  for (i = 0; i < (int)vk->num_frames; ++i) {
    VkBuffer buffer = vk->render_buffers[i];
    owl_vector(VkBuffer) *buffers = &vk->garbage_buffers[i];

    code = owl_vector_push_back(buffers, buffer);
    if (code)
      goto error_pop_buffers;
  }

  for (i = 0; i < (int)vk->num_frames; ++i) {
    VkDeviceMemory memory = vk->render_buffer_memories[i];
    owl_vector(VkDeviceMemory) *memories = &vk->garbage_memories[i];

    code = owl_vector_push_back(memories, memory);
    if (code)
      goto error_pop_memories;
  }

  for (i = 0; i < (int)vk->num_frames; ++i) {
    VkDescriptorSet set = vk->render_buffer_pvm_sets[i];
    owl_vector(VkDescriptorSet) *sets = &vk->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_pvm_sets;
  }

  for (i = 0; i < (int)vk->num_frames; ++i) {
    VkDescriptorSet set = vk->render_buffer_model1_sets[i];
    owl_vector(VkDescriptorSet) *sets = &vk->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_model1_sets;
  }

  for (i = 0; i < (int)vk->num_frames; ++i) {
    VkDescriptorSet set = vk->render_buffer_model2_sets[i];
    owl_vector(VkDescriptorSet) *sets = &vk->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_model2_sets;
  }

  goto out;

error_pop_model2_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  i = vk->num_frames;

error_pop_model1_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  i = vk->num_frames;

error_pop_pvm_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  i = vk->num_frames;

error_pop_memories:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(vk->garbage_memories[i]);

  i = vk->num_frames;

error_pop_buffers:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(vk->garbage_buffers[i]);

out:
  return code;
}

owl_private owl_code
owl_vk_frame_pop_garbage(struct owl_vk_renderer *vk)
{
  uint32_t i;

  for (i = 0; i < vk->num_frames; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  for (i = 0; i < vk->num_frames; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  for (i = 0; i < vk->num_frames; ++i)
    owl_vector_pop(vk->garbage_sets[i]);

  for (i = 0; i < vk->num_frames; ++i)
    owl_vector_pop(vk->garbage_memories[i]);

  for (i = 0; i < vk->num_frames; ++i)
    owl_vector_pop(vk->garbage_buffers[i]);

  return OWL_OK;
}

owl_public owl_code
owl_vk_frame_reserve(struct owl_vk_renderer *vk, uint64_t size)
{
  if (vk->render_buffer_size < (size + vk->render_buffer_offset)) {
    uint64_t nsize;
    owl_code code;

    code = owl_vk_frame_push_garbage(vk);
    if (code)
      return code;

    nsize = (size + vk->render_buffer_offset) * 2;
    nsize = owl_alignu2(nsize, vk->render_buffer_alignment);

    code = owl_vk_renderer_init_render_buffers(vk, nsize);
    if (code) {
      owl_vk_frame_pop_garbage(vk);
      return code;
    }
  }

  return OWL_OK;
}

owl_public void *
owl_vk_frame_allocate(struct owl_vk_renderer *vk, uint64_t size,
                      struct owl_vk_frame_allocation *alloc)
{
  owl_code code;

  uint8_t *data = NULL;

  code = owl_vk_frame_reserve(vk, size);
  if (!code) {
    uint64_t offset = vk->render_buffer_offset;

    vk->render_buffer_offset = owl_alignu2(offset + size,
                                           vk->render_buffer_alignment);

    data = &((uint8_t *)vk->render_buffer_data[vk->frame])[offset];

    alloc->offset32 = (uint32_t)offset;
    alloc->offset = offset;
    alloc->buffer = vk->render_buffers[vk->frame];
    alloc->pvm_set = vk->render_buffer_pvm_sets[vk->frame];
    alloc->model1_set = vk->render_buffer_model1_sets[vk->frame];
    alloc->model2_set = vk->render_buffer_model2_sets[vk->frame];
  }

  return data;
}

#define owl_vk_is_swapchain_out_of_date(vk_result)                            \
  (VK_ERROR_OUT_OF_DATE_KHR == (vk_result) ||                                 \
   VK_SUBOPTIMAL_KHR == (vk_result) ||                                        \
   VK_ERROR_SURFACE_LOST_KHR == (vk_result))

owl_public owl_code
owl_vk_frame_begin(struct owl_vk_renderer *vk)
{
  VkSemaphore acquire_semaphore;
  VkFence in_flight_fence;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkCommandBufferBeginInfo command_buffer_info;
  VkFramebuffer framebuffer;
  VkRenderPassBeginInfo pass_info;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  owl_vk_renderer_bind_pipeline(vk, OWL_VK_PIPELINE_NONE);

  acquire_semaphore = vk->frame_acquire_semaphores[vk->frame];
  vk_result = vkAcquireNextImageKHR(vk->device, vk->swapchain, (uint64_t)-1,
                                    acquire_semaphore, VK_NULL_HANDLE,
                                    &vk->swapchain_image);
  if (owl_vk_is_swapchain_out_of_date(vk_result)) {
    code = owl_vk_renderer_resize_swapchain(vk);
    if (code)
      return code;

    vk_result = vkAcquireNextImageKHR(vk->device, vk->swapchain, (uint64_t)-1,
                                      acquire_semaphore, VK_NULL_HANDLE,
                                      &vk->swapchain_image);
  } else if (vk_result) {
    return OWL_ERROR_FATAL;
  }

  in_flight_fence = vk->frame_in_flight_fences[vk->frame];
  vk_result = vkWaitForFences(vk->device, 1, &in_flight_fence, VK_TRUE,
                              (uint64_t)-1);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vk_result = vkResetFences(vk->device, 1, &in_flight_fence);
  if (vk_result)
    return OWL_ERROR_FATAL;

  command_pool = vk->frame_command_pools[vk->frame];
  vk_result = vkResetCommandPool(vk->device, command_pool, 0);
  if (vk_result)
    return OWL_ERROR_FATAL;

  owl_vk_frame_collect_garbage(vk);

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_info.pInheritanceInfo = NULL;

  command_buffer = vk->frame_command_buffers[vk->frame];
  vk_result = vkBeginCommandBuffer(command_buffer, &command_buffer_info);
  if (vk_result)
    return OWL_ERROR_FATAL;

  framebuffer = vk->swapchain_framebuffers[vk->swapchain_image];

  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass_info.pNext = NULL;
  pass_info.renderPass = vk->main_render_pass;
  pass_info.framebuffer = framebuffer;
  pass_info.renderArea.offset.x = 0;
  pass_info.renderArea.offset.y = 0;
  pass_info.renderArea.extent.width = vk->width;
  pass_info.renderArea.extent.height = vk->height;
  pass_info.clearValueCount = owl_array_size(vk->clear_values);
  pass_info.pClearValues = vk->clear_values;

  vkCmdBeginRenderPass(command_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

  return OWL_OK;
}

owl_public owl_code
owl_vk_frame_end(struct owl_vk_renderer *vk)
{
  VkCommandBuffer command_buffer;
  VkPipelineStageFlagBits stage;
  VkSemaphore acquire_semaphore;
  VkSemaphore render_done_semaphore;
  VkSubmitInfo submit_info;
  VkPresentInfoKHR present_info;
  VkResult vk_result;

  command_buffer = vk->frame_command_buffers[vk->frame];

  vkCmdEndRenderPass(command_buffer);

  vk_result = vkEndCommandBuffer(command_buffer);
  if (vk_result)
    return OWL_ERROR_FATAL;

  stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  acquire_semaphore = vk->frame_acquire_semaphores[vk->frame];
  render_done_semaphore = vk->frame_render_done_semaphores[vk->frame];

  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &acquire_semaphore;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_done_semaphore;
  submit_info.pWaitDstStageMask = &stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vk_result = vkQueueSubmit(vk->graphics_queue, 1, &submit_info,
                            vk->frame_in_flight_fences[vk->frame]);
  if (vk_result)
    return OWL_ERROR_FATAL;

  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_done_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &vk->swapchain;
  present_info.pImageIndices = &vk->swapchain_image;
  present_info.pResults = NULL;

  vk_result = vkQueuePresentKHR(vk->present_queue, &present_info);
  if (owl_vk_is_swapchain_out_of_date(vk_result))
    return owl_vk_renderer_resize_swapchain(vk);

  if (vk->num_frames == ++vk->frame)
    vk->frame = 0;

  vk->render_buffer_offset = 0;

  return OWL_OK;
}

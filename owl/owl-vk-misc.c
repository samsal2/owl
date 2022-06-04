#include "owl-vk-misc.h"

#include "owl-internal.h"
#include "owl-vk-renderer.h"

owl_public owl_u32
owl_vk_find_memory_type(struct owl_vk_renderer *vk, owl_u32 filter,
                        owl_u32 prop)
{
  owl_u32 ty;
  VkPhysicalDeviceMemoryProperties memprops;

  vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &memprops);

  for (ty = 0; ty < memprops.memoryTypeCount; ++ty) {
    owl_u32 cur = memprops.memoryTypes[ty].propertyFlags;

    if ((cur & prop) && (filter & (1U << ty)))
      return ty;
  }

  return (owl_u32)-1;
}

owl_public enum owl_code
owl_vk_begin_im_command_buffer(struct owl_vk_renderer *vk)
{
  VkCommandBufferAllocateInfo command_buffer_info;
  VkCommandBufferBeginInfo begin_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  owl_assert(!vk->im_command_buffer);

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.commandPool = vk->command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;

  vk_result = vkAllocateCommandBuffers(vk->device, &command_buffer_info,
                                       &vk->im_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = NULL;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;

  vk_result = vkBeginCommandBuffer(vk->im_command_buffer, &begin_info);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_im_command_buffer_deinit;
  }

  goto out;

error_im_command_buffer_deinit:
  vkFreeCommandBuffers(vk->device, vk->command_pool, 1,
                       &vk->im_command_buffer);

out:
  return code;
}

owl_public enum owl_code
owl_vk_end_im_command_buffer(struct owl_vk_renderer *vk)
{
  VkSubmitInfo submit_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  owl_assert(vk->im_command_buffer);

  vk_result = vkEndCommandBuffer(vk->im_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = NULL;
  submit_info.pWaitDstStageMask = NULL;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &vk->im_command_buffer;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;

  vk_result = vkQueueSubmit(vk->graphics_queue, 1, &submit_info, NULL);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  vk_result = vkQueueWaitIdle(vk->graphics_queue);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  vk->im_command_buffer = VK_NULL_HANDLE;

cleanup:
  vkFreeCommandBuffers(vk->device, vk->command_pool, 1,
                       &vk->im_command_buffer);

  return code;
}

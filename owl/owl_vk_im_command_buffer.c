#include "owl_vk_im_command_buffer.h"

#include "owl_vk_context.h"

owl_public enum owl_code
owl_vk_im_command_buffer_begin (struct owl_vk_im_command_buffer *cmd,
                                struct owl_vk_context const     *ctx)
{
  VkCommandBufferAllocateInfo command_buffer_info;
  VkCommandBufferBeginInfo    begin_info;

  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.commandPool        = ctx->vk_command_pool;
  command_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;

  vk_result = vkAllocateCommandBuffers (ctx->vk_device, &command_buffer_info,
                                        &cmd->vk_command_buffer);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext            = NULL;
  begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;

  vk_result = vkBeginCommandBuffer (cmd->vk_command_buffer, &begin_info);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto error_command_buffer_deinit;
  }

  goto out;

error_command_buffer_deinit:
  vkFreeCommandBuffers (ctx->vk_device, ctx->vk_command_pool, 1,
                        &cmd->vk_command_buffer);

out:
  return code;
}

owl_public enum owl_code
owl_vk_im_command_buffer_end (struct owl_vk_im_command_buffer *cmd,
                              struct owl_vk_context const     *ctx)
{
  VkSubmitInfo info;

  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  vk_result = vkEndCommandBuffer (cmd->vk_command_buffer);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext                = NULL;
  info.waitSemaphoreCount   = 0;
  info.pWaitSemaphores      = NULL;
  info.pWaitDstStageMask    = NULL;
  info.commandBufferCount   = 1;
  info.pCommandBuffers      = &cmd->vk_command_buffer;
  info.signalSemaphoreCount = 0;
  info.pSignalSemaphores    = NULL;

  vk_result = vkQueueSubmit (ctx->vk_graphics_queue, 1, &info, NULL);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out_command_buffer_deinit;
  }

  vk_result = vkQueueWaitIdle (ctx->vk_graphics_queue);
  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out_command_buffer_deinit;
  }

out_command_buffer_deinit:
  vkFreeCommandBuffers (ctx->vk_device, ctx->vk_command_pool, 1,
                        &cmd->vk_command_buffer);

out:
  return code;
}

#ifndef OWL_VK_IM_COMMAND_BUFFER_H_
#define OWL_VK_IM_COMMAND_BUFFER_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;

struct owl_vk_im_command_buffer
{
  VkCommandBuffer vk_command_buffer;
};

owl_public enum owl_code
owl_vk_im_command_buffer_begin (struct owl_vk_im_command_buffer *cmd,
                                struct owl_vk_context const     *ctx);

owl_public enum owl_code
owl_vk_im_command_buffer_end (struct owl_vk_im_command_buffer *cmd,
                              struct owl_vk_context const     *ctx);

OWL_END_DECLS

#endif

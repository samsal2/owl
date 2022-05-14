#ifndef OWL_VK_ATTACHMENT_H_
#define OWL_VK_ATTACHMENT_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;

enum owl_vk_attachment_type {
  OWL_VK_ATTACHMENT_TYPE_COLOR,
  OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL
};

struct owl_vk_attachment {
  owl_u32 width;
  owl_u32 height;
  VkImage vk_image;
  VkDeviceMemory vk_memory;
  VkImageView vk_image_view;
};

owl_public enum owl_code
owl_vk_attachment_init (struct owl_vk_attachment *attachment,
                        struct owl_vk_context *ctx, owl_i32 w, owl_i32 h,
                        enum owl_vk_attachment_type type);

owl_public void
owl_vk_attachment_deinit (struct owl_vk_attachment *attachment,
                          struct owl_vk_context *ctx);

OWL_BEGIN_DECLS

#endif

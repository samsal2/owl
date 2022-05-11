#include "owl_vk_attachment.h"

#include "owl_vk_context.h"

owl_private VkFormat
owl_vk_attachment_type_get_format (enum owl_vk_attachment_type type,
                                   struct owl_vk_context const *ctx)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return ctx->vk_surface_format.format;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return ctx->vk_depth_stencil_format;
  }
}

owl_private VkImageUsageFlagBits
owl_vk_attachment_type_get_usage (enum owl_vk_attachment_type type)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
}

owl_private VkImageAspectFlagBits
owl_vk_attachment_type_get_aspect (enum owl_vk_attachment_type type)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return VK_IMAGE_ASPECT_COLOR_BIT;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  }
}

owl_public enum owl_code
owl_vk_attachment_init (struct owl_vk_attachment *attachment,
                        struct owl_vk_context *ctx, owl_i32 w, owl_i32 h,
                        enum owl_vk_attachment_type type)
{
  VkImageCreateInfo image_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo memory_info;
  VkImageViewCreateInfo image_view_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  attachment->width = w;
  attachment->height = h;

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = owl_vk_attachment_type_get_format (type, ctx);
  image_info.extent.width = w;
  image_info.extent.height = h;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = ctx->msaa;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = owl_vk_attachment_type_get_usage (type);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result =
      vkCreateImage (ctx->vk_device, &image_info, NULL, &attachment->vk_image);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkGetImageMemoryRequirements (ctx->vk_device, attachment->vk_image, &req);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = req.size;
  memory_info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_GPU_ONLY);

  vk_result = vkAllocateMemory (ctx->vk_device, &memory_info, NULL,
                                &attachment->vk_memory);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_image_deinit;
  }

  vk_result = vkBindImageMemory (ctx->vk_device, attachment->vk_image,
                                 attachment->vk_memory, 0);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = attachment->vk_image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = image_info.format;
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.subresourceRange.aspectMask =
      owl_vk_attachment_type_get_aspect (type);
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  vk_result = vkCreateImageView (ctx->vk_device, &image_view_info, NULL,
                                 &attachment->vk_image_view);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  goto out;

out_error_memory_deinit:
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);

out_error_image_deinit:
  vkDestroyImage (ctx->vk_device, attachment->vk_image, NULL);

out:
  return code;
}

owl_public void
owl_vk_attachment_deinit (struct owl_vk_attachment *attachment,
                          struct owl_vk_context *ctx)
{
  vkDestroyImageView (ctx->vk_device, attachment->vk_image_view, NULL);
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);
  vkDestroyImage (ctx->vk_device, attachment->vk_image, NULL);
}

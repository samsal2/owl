#include "owl_vk_attachment.h"

#include "owl_vk_context.h"

owl_private VkFormat
owl_vk_attachment_type_get_format (enum owl_vk_attachment_type  type,
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
owl_vk_attachment_image_init (struct owl_vk_attachment   *attachment,
                              struct owl_vk_context      *ctx,
                              owl_i32                     w,
                              owl_i32                     h,
                              enum owl_vk_attachment_type type)
{
  VkImageCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext                 = NULL;
  info.flags                 = 0;
  info.imageType             = VK_IMAGE_TYPE_2D;
  info.format                = owl_vk_attachment_type_get_format (type, ctx);
  info.extent.width          = w;
  info.extent.height         = h;
  info.extent.depth          = 1;
  info.mipLevels             = 1;
  info.arrayLayers           = 1;
  info.samples               = ctx->msaa;
  info.tiling                = VK_IMAGE_TILING_OPTIMAL;
  info.usage                 = owl_vk_attachment_type_get_usage (type);
  info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices   = NULL;
  info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result =
      vkCreateImage (ctx->vk_device, &info, NULL, &attachment->vk_image);

  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_attachment_image_deinit (struct owl_vk_attachment    *attachment,
                                struct owl_vk_context const *ctx)
{
  vkDestroyImage (ctx->vk_device, attachment->vk_image, NULL);
}

owl_private enum owl_code
owl_vk_attachment_memory_init (struct owl_vk_attachment *attachment,
                               struct owl_vk_context    *ctx)
{
  VkMemoryRequirements req;
  VkMemoryAllocateInfo info;

  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  vkGetImageMemoryRequirements (ctx->vk_device, attachment->vk_image, &req);

  info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.pNext           = NULL;
  info.allocationSize  = req.size;
  info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_GPU_ONLY);

  vk_result =
      vkAllocateMemory (ctx->vk_device, &info, NULL, &attachment->vk_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vk_result = vkBindImageMemory (ctx->vk_device, attachment->vk_image,
                                 attachment->vk_memory, 0);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  goto out;

out_error_memory_deinit:
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);

out:
  return code;
}

owl_private void
owl_vk_attachment_memory_deinit (struct owl_vk_attachment    *attachment,
                                 struct owl_vk_context const *ctx)
{
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);
}

owl_private enum owl_code
owl_vk_attachment_image_view_init (struct owl_vk_attachment   *attachment,
                                   struct owl_vk_context      *ctx,
                                   enum owl_vk_attachment_type type)
{
  VkImageViewCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext        = NULL;
  info.flags        = 0;
  info.image        = attachment->vk_image;
  info.viewType     = VK_IMAGE_VIEW_TYPE_2D;
  info.format       = owl_vk_attachment_type_get_format (type, ctx);
  info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.subresourceRange.aspectMask = owl_vk_attachment_type_get_aspect (type);
  info.subresourceRange.baseMipLevel   = 0;
  info.subresourceRange.levelCount     = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount     = 1;

  vk_result = vkCreateImageView (ctx->vk_device, &info, NULL,
                                 &attachment->vk_image_view);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_attachment_image_view_deinit (struct owl_vk_attachment    *attachment,
                                     struct owl_vk_context const *ctx)
{
  vkDestroyImageView (ctx->vk_device, attachment->vk_image_view, NULL);
}

owl_public enum owl_code
owl_vk_attachment_init (struct owl_vk_attachment   *attachment,
                        struct owl_vk_context      *ctx,
                        owl_i32                     w,
                        owl_i32                     h,
                        enum owl_vk_attachment_type type)
{
  enum owl_code code;

  attachment->width  = w;
  attachment->height = h;

  code = owl_vk_attachment_image_init (attachment, ctx, w, h, type);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_attachment_memory_init (attachment, ctx);
  if (OWL_SUCCESS != code)
    goto out_error_image_deinit;

  code = owl_vk_attachment_image_view_init (attachment, ctx, type);
  if (OWL_SUCCESS != code)
    goto out_error_memory_deinit;

  goto out;

out_error_memory_deinit:
  owl_vk_attachment_memory_deinit (attachment, ctx);

out_error_image_deinit:
  owl_vk_attachment_image_deinit (attachment, ctx);

out:
  return code;
}

owl_public void
owl_vk_attachment_deinit (struct owl_vk_attachment *attachment,
                          struct owl_vk_context    *ctx)
{
  owl_vk_attachment_image_view_deinit (attachment, ctx);
  owl_vk_attachment_memory_deinit (attachment, ctx);
  owl_vk_attachment_image_deinit (attachment, ctx);
}

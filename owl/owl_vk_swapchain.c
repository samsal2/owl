#include "owl_vk_swapchain.h"

#include "owl_internal.h"
#include "owl_vk_attachment.h"
#include "owl_vk_context.h"
#include "owl_vk_frame_sync.h"

#define OWL_UNRESTRICTED_DIMENSION (owl_u32) - 1

owl_private enum owl_code
owl_vk_swapchain_size_ensure (struct owl_vk_swapchain *swapchain,
                              struct owl_vk_context const *ctx)
{
  VkSurfaceCapabilitiesKHR capabilities;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR (
      ctx->vk_physical_device, ctx->vk_surface, &capabilities);
  if (VK_SUCCESS != vk_result) {
    code = OWL_SUCCESS;
    goto out;
  }

  if (OWL_UNRESTRICTED_DIMENSION == capabilities.currentExtent.width) {
    swapchain->size = capabilities.currentExtent;
  } else {
    owl_u32 const min_width = capabilities.minImageExtent.width;
    owl_u32 const min_height = capabilities.minImageExtent.height;
    owl_u32 const max_width = capabilities.maxImageExtent.width;
    owl_u32 const max_height = capabilities.maxImageExtent.height;

    swapchain->size.width =
        owl_clamp (swapchain->size.width, min_width, max_width);

    swapchain->size.height =
        owl_clamp (swapchain->size.height, min_height, max_height);
  }

out:
  return code;
}

owl_private enum owl_code
owl_vk_swapchain_swapchain_init (struct owl_vk_swapchain *swapchain,
                                 struct owl_vk_context const *ctx, owl_u32 w,
                                 owl_u32 h)
{
  owl_u32 families[2];
  VkSwapchainCreateInfoKHR info;
  VkSurfaceCapabilitiesKHR capabilities;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  families[0] = ctx->graphics_queue_family;
  families[1] = ctx->present_queue_family;

  swapchain->size.width = w;
  swapchain->size.height = h;
  code = owl_vk_swapchain_size_ensure (swapchain, ctx);
  if (OWL_SUCCESS != code)
    return code;

  vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR (
      ctx->vk_physical_device, ctx->vk_surface, &capabilities);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.pNext = NULL;
  info.flags = 0;
  info.surface = ctx->vk_surface;
  info.minImageCount = OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT;
  info.imageFormat = ctx->vk_surface_format.format;
  info.imageColorSpace = ctx->vk_surface_format.colorSpace;
  info.imageExtent.width = swapchain->size.width;
  info.imageExtent.height = swapchain->size.height;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  info.preTransform = capabilities.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = ctx->vk_present_mode;
  info.clipped = VK_TRUE;
  info.oldSwapchain = VK_NULL_HANDLE;

  if (ctx->graphics_queue_family == ctx->present_queue_family) {
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
  } else {
    info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 2;
    info.pQueueFamilyIndices = families;
  }

  vk_result = vkCreateSwapchainKHR (ctx->vk_device, &info, NULL,
                                    &swapchain->vk_swapchain);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_swapchain_vk_swapchain_deinit (struct owl_vk_swapchain *swapchain,
                                      struct owl_vk_context const *ctx)
{
  vkDestroySwapchainKHR (ctx->vk_device, swapchain->vk_swapchain, NULL);
}

owl_private enum owl_code
owl_vk_swapchain_images_init (struct owl_vk_swapchain *swapchain,
                              struct owl_vk_context const *ctx)
{
  VkResult vk_result = VK_SUCCESS;

  vk_result = vkGetSwapchainImagesKHR (ctx->vk_device, swapchain->vk_swapchain,
                                       &swapchain->vk_image_count, NULL);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  if (OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT <= swapchain->vk_image_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  vk_result = vkGetSwapchainImagesKHR (ctx->vk_device, swapchain->vk_swapchain,
                                       &swapchain->vk_image_count,
                                       swapchain->vk_images);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_swapchain_image_views_init (struct owl_vk_swapchain *swapchain,
                                   struct owl_vk_context const *ctx)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)swapchain->vk_image_count; ++i) {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = swapchain->vk_images[i];
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = ctx->vk_surface_format.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView (ctx->vk_device, &info, NULL,
                                   &swapchain->vk_image_views[i]);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_image_views_deinit;
    }
  }

  goto out;

out_error_image_views_deinit:
  for (i = i - 1; i >= 0; --i)
    vkDestroyImageView (ctx->vk_device, swapchain->vk_image_views[i], NULL);

out:
  return code;
}

owl_private void
owl_vk_swapchain_image_views_deinit (struct owl_vk_swapchain *swapchain,
                                     struct owl_vk_context const *ctx)
{
  owl_i32 i;
  for (i = 0; i < (owl_i32)swapchain->vk_image_count; ++i)
    vkDestroyImageView (ctx->vk_device, swapchain->vk_image_views[i], NULL);
}

owl_private enum owl_code
owl_vk_swapchain_framebuffers_init (
    struct owl_vk_swapchain *swapchain, struct owl_vk_context const *ctx,
    struct owl_vk_attachment const *color,
    struct owl_vk_attachment const *depth_stencil)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)swapchain->vk_image_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo info;

    attachments[0] = color->vk_image_view;
    attachments[1] = depth_stencil->vk_image_view;
    attachments[2] = swapchain->vk_image_views[i];

    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.renderPass = ctx->vk_main_render_pass;
    info.attachmentCount = owl_array_size (attachments);
    info.pAttachments = attachments;
    info.width = swapchain->size.width;
    info.height = swapchain->size.height;
    info.layers = 1;

    vk_result = vkCreateFramebuffer (ctx->vk_device, &info, NULL,
                                     &swapchain->vk_framebuffers[i]);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_framebuffers_deinit;
    }
  }

  goto out;

out_error_framebuffers_deinit:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFramebuffer (ctx->vk_device, swapchain->vk_framebuffers[i], NULL);

out:
  return code;
}

owl_private void
owl_vk_swapchain_framebuffer_deinit (struct owl_vk_swapchain *swapchain,
                                     struct owl_vk_context const *ctx)
{
  owl_i32 i;
  for (i = 0; i < (owl_i32)swapchain->vk_image_count; ++i)
    vkDestroyFramebuffer (ctx->vk_device, swapchain->vk_framebuffers[i], NULL);
}

owl_public enum owl_code
owl_vk_swapchain_init (struct owl_vk_swapchain *swapchain,
                       struct owl_vk_context const *ctx,
                       struct owl_vk_attachment const *color,
                       struct owl_vk_attachment const *depth_stencil)
{
  owl_i32 w;
  owl_i32 h;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (color->width == depth_stencil->width);
  owl_assert (color->height == depth_stencil->height);

  swapchain->image = 0;
  w = color->width;
  h = color->height;
  code = owl_vk_swapchain_swapchain_init (swapchain, ctx, w, h);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_swapchain_images_init (swapchain, ctx);
  if (OWL_SUCCESS != code)
    goto out_error_vk_swapchain_deinit;

  code = owl_vk_swapchain_image_views_init (swapchain, ctx);
  if (OWL_SUCCESS != code)
    goto out_error_vk_swapchain_deinit;

  code = owl_vk_swapchain_framebuffers_init (swapchain, ctx, color,
                                             depth_stencil);
  if (OWL_SUCCESS != code)
    goto out_error_image_views_deinit;

  swapchain->clear_values[0].color.float32[0] = 0.0F;
  swapchain->clear_values[0].color.float32[1] = 0.0F;
  swapchain->clear_values[0].color.float32[2] = 0.0F;
  swapchain->clear_values[0].color.float32[3] = 1.0F;
  swapchain->clear_values[1].depthStencil.depth = 1.0F;
  swapchain->clear_values[1].depthStencil.stencil = 0.0F;

  goto out;

out_error_image_views_deinit:
  owl_vk_swapchain_image_views_deinit (swapchain, ctx);

out_error_vk_swapchain_deinit:
  owl_vk_swapchain_vk_swapchain_deinit (swapchain, ctx);

out:
  return code;
}

owl_public void
owl_vk_swapchain_deinit (struct owl_vk_swapchain *swapchain,
                         struct owl_vk_context const *ctx)
{
  owl_vk_swapchain_framebuffer_deinit (swapchain, ctx);
  owl_vk_swapchain_image_views_deinit (swapchain, ctx);
  owl_vk_swapchain_vk_swapchain_deinit (swapchain, ctx);
}

owl_public enum owl_code
owl_vk_swapchain_acquire_next_image (struct owl_vk_swapchain *swapchain,
                                     struct owl_vk_context const *ctx,
                                     struct owl_vk_frame_sync const *sync)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkAcquireNextImageKHR (
      ctx->vk_device, swapchain->vk_swapchain, (owl_u64)-1,
      sync->vk_image_available_semaphore, VK_NULL_HANDLE, &swapchain->image);

  if (VK_ERROR_OUT_OF_DATE_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUCCESS == vk_result) {
    code = OWL_SUCCESS;
  } else {
    code = OWL_ERROR_UNKNOWN;
  }

  return code;
}

owl_public enum owl_code
owl_vk_swapchain_present (struct owl_vk_swapchain *swapchain,
                          struct owl_vk_context const *ctx,
                          struct owl_vk_frame_sync const *sync)
{
  VkPresentInfoKHR info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = NULL;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &sync->vk_render_done_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &swapchain->vk_swapchain;
  info.pImageIndices = &swapchain->image;
  info.pResults = NULL;

  vk_result = vkQueuePresentKHR (ctx->vk_present_queue, &info);

  if (VK_ERROR_OUT_OF_DATE_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  }

  return code;
}

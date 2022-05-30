#include "owl_vk_image.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_im_command_buffer.h"
#include "owl_vk_pipeline_manager.h"
#include "owl_vk_stage_heap.h"
#include "stb_image.h"

#include <math.h>

struct owl_vk_image_load {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  enum owl_pixel_format format;
  struct owl_vk_stage_allocation allocation;
};

owl_private owl_u64
owl_pixel_format_size (enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof (owl_u8);

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof (owl_u8);
  }
}

owl_private owl_u32
owl_vk_image_calculate_mips (owl_u32 w, owl_u32 h) {
  return (owl_u32)(floor (log2 (owl_max (w, h))) + 1);
}

owl_private enum owl_code
owl_vk_image_load_init_from_file (struct owl_vk_image_load *load,
                                  struct owl_vk_context const *ctx,
                                  struct owl_vk_stage_heap *heap,
                                  struct owl_vk_image_desc const *desc) {
  owl_i32 width;
  owl_i32 height;
  owl_i32 chans;
  owl_u64 size;
  owl_byte *data;
  owl_byte *stage_data;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (OWL_VK_IMAGE_SRC_TYPE_FILE == desc->src_type);

  data = stbi_load (desc->src_path, &width, &height, &chans, STBI_rgb_alpha);
  if (!data) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  load->format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;
  load->width = (owl_u32)width;
  load->height = (owl_u32)height;
  load->mips = owl_vk_image_calculate_mips (load->width, load->height);

  size = load->width * load->height * owl_pixel_format_size (load->format);

  stage_data = owl_vk_stage_heap_allocate (heap, ctx, size, &load->allocation);
  if (!stage_data) {
    code = OWL_ERROR_UNKNOWN;
    goto out_data_deinit;
  }
  owl_memcpy (stage_data, data, size);

out_data_deinit:
  stbi_image_free (data);

out:
  return code;
}

owl_private enum owl_code
owl_vk_image_load_init_from_data (struct owl_vk_image_load *load,
                                  struct owl_vk_context const *ctx,
                                  struct owl_vk_stage_heap *heap,
                                  struct owl_vk_image_desc const *desc) {
  owl_u64 size;
  owl_byte *stage_data;

  owl_assert (OWL_VK_IMAGE_SRC_TYPE_DATA == desc->src_type);

  load->format = desc->src_data_pixel_format;
  load->width = (owl_u32)desc->src_data_width;
  load->height = (owl_u32)desc->src_data_height;
  load->mips = owl_vk_image_calculate_mips (load->width, load->height);

  size = load->width * load->height * owl_pixel_format_size (load->format);
  stage_data = owl_vk_stage_heap_allocate (heap, ctx, size, &load->allocation);
  if (!stage_data)
    return OWL_ERROR_UNKNOWN;
  owl_memcpy (stage_data, desc->src_data, size);

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_image_load_init (struct owl_vk_image_load *load,
                        struct owl_vk_context const *ctx,
                        struct owl_vk_stage_heap *heap,
                        struct owl_vk_image_desc const *desc) {
  switch (desc->src_type) {
  case OWL_VK_IMAGE_SRC_TYPE_DATA:
    return owl_vk_image_load_init_from_data (load, ctx, heap, desc);

  case OWL_VK_IMAGE_SRC_TYPE_FILE:
    return owl_vk_image_load_init_from_file (load, ctx, heap, desc);
  }
}

owl_private void
owl_vk_image_load_deinit (struct owl_vk_image_load *load,
                          struct owl_vk_context const *ctx,
                          struct owl_vk_stage_heap *heap) {
  owl_unused (load);
  owl_vk_stage_heap_free (heap, ctx);
}

owl_private VkFormat
owl_as_vk_format (enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

owl_private VkFilter
owl_as_vk_filter (enum owl_sampler_filter filter) {
  switch (filter) {
  case OWL_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

owl_private VkSamplerMipmapMode
owl_as_vk_sampler_mipmap_mode (enum owl_sampler_mip_mode mode) {
  switch (mode) {
  case OWL_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

owl_private VkSamplerAddressMode
owl_as_vk_sampler_address_mode (enum owl_sampler_address_mode mode) {
  switch (mode) {
  case OWL_SAMPLER_ADDRESS_MODE_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;

  case OWL_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

  case OWL_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  case OWL_SAMPLER_ADDRESS_MODEL_CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }
}

owl_private void
owl_vk_image_generate_mips (struct owl_vk_image const *image,
                            struct owl_vk_im_command_buffer const *cmd,
                            owl_i32 width, owl_i32 height, owl_i32 mips) {
  owl_i32 i;

  VkImageMemoryBarrier barrier;

  if (1 == mips || 0 == mips)
    return;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image->vk_image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (owl_i32)(mips - 1); ++i) {
    VkImageBlit blit;

    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier (
        cmd->vk_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    blit.srcOffsets[0].x = 0;
    blit.srcOffsets[0].y = 0;
    blit.srcOffsets[0].z = 0;
    blit.srcOffsets[1].x = width;
    blit.srcOffsets[1].y = height;
    blit.srcOffsets[1].z = 1;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0].x = 0;
    blit.dstOffsets[0].y = 0;
    blit.dstOffsets[0].z = 0;
    blit.dstOffsets[1].x = (width > 1) ? (width /= 2) : 1;
    blit.dstOffsets[1].y = (height > 1) ? (height /= 2) : 1;
    blit.dstOffsets[1].z = 1;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i + 1;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage (cmd->vk_command_buffer, image->vk_image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image->vk_image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                    VK_FILTER_LINEAR);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier (cmd->vk_command_buffer,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                          NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier (cmd->vk_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                        NULL, 1, &barrier);
}

owl_private void
owl_vk_image_transition (struct owl_vk_image const *image,
                         struct owl_vk_im_command_buffer const *cmd,
                         owl_u32 mips, owl_u32 layers,
                         VkImageLayout src_layout, VkImageLayout dst_layout) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = src_layout;
  barrier.newLayout = dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image->vk_image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == src_layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    owl_assert (0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    owl_assert (0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier (cmd->vk_command_buffer, src, dst, 0, 0, NULL, 0, NULL,
                        1, &barrier);
}

owl_private enum owl_code
owl_vk_image_image_init (struct owl_vk_image *image,
                         struct owl_vk_context const *ctx,
                         struct owl_vk_image_load const *load) {
  VkImageCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.imageType = VK_IMAGE_TYPE_2D;
  info.format = owl_as_vk_format (load->format);
  info.extent.width = load->width;
  info.extent.height = load->height;
  info.extent.depth = 1;
  info.mipLevels = load->mips;
  info.arrayLayers = 1;
  info.samples = VK_SAMPLE_COUNT_1_BIT;
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices = NULL;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result = vkCreateImage (ctx->vk_device, &info, NULL, &image->vk_image);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_image_image_deinit (struct owl_vk_image *image,
                           struct owl_vk_context const *ctx) {
  vkDestroyImage (ctx->vk_device, image->vk_image, NULL);
}

owl_private enum owl_code
owl_vk_image_memory_init (struct owl_vk_image *image,
                          struct owl_vk_context const *ctx) {
  VkMemoryRequirements req;
  VkMemoryAllocateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkGetImageMemoryRequirements (ctx->vk_device, image->vk_image, &req);

  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.pNext = NULL;
  info.allocationSize = req.size;
  info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_GPU_ONLY);

  vk_result =
      vkAllocateMemory (ctx->vk_device, &info, NULL, &image->vk_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vk_result =
      vkBindImageMemory (ctx->vk_device, image->vk_image, image->vk_memory, 0);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  goto out;

error_memory_deinit:
  vkFreeMemory (ctx->vk_device, image->vk_memory, NULL);

out:
  return code;
}

owl_private void
owl_vk_image_memory_deinit (struct owl_vk_image *image,
                            struct owl_vk_context const *ctx) {
  vkFreeMemory (ctx->vk_device, image->vk_memory, NULL);
}

owl_private enum owl_code
owl_vk_image_image_view_init (struct owl_vk_image *image,
                              struct owl_vk_context const *ctx,
                              struct owl_vk_image_load const *load) {
  VkImageViewCreateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.image = image->vk_image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = owl_as_vk_format (load->format);
  info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = load->mips;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;

  vk_result =
      vkCreateImageView (ctx->vk_device, &info, NULL, &image->vk_image_view);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_private void
owl_vk_image_image_view_deinit (struct owl_vk_image *image,
                                struct owl_vk_context const *ctx) {
  vkDestroyImageView (ctx->vk_device, image->vk_image_view, NULL);
}

owl_private enum owl_code
owl_vk_image_sampler_init (struct owl_vk_image *image,
                           struct owl_vk_context const *ctx,
                           struct owl_vk_image_desc const *desc,
                           struct owl_vk_image_load const *load) {
  VkSamplerCreateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  if (desc->use_default_sampler) {
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.mipLodBias = 0.0F;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = 16.0F;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.minLod = 0.0F;
    info.maxLod = load->mips; /* VK_LOD_CLAMP_NONE */
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
  } else {
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.magFilter = owl_as_vk_filter (desc->sampler_mag_filter);
    info.minFilter = owl_as_vk_filter (desc->sampler_min_filter);
    info.mipmapMode = owl_as_vk_sampler_mipmap_mode (desc->sampler_mip_mode);
    info.addressModeU = owl_as_vk_sampler_address_mode (desc->sampler_wrap_u);
    info.addressModeV = owl_as_vk_sampler_address_mode (desc->sampler_wrap_v);
    info.addressModeW = owl_as_vk_sampler_address_mode (desc->sampler_wrap_w);
    info.mipLodBias = 0.0F;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = 16.0F;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.minLod = 0.0F;
    info.maxLod = load->mips; /* VK_LOD_CLAMP_NONE */
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
  }

  vk_result =
      vkCreateSampler (ctx->vk_device, &info, NULL, &image->vk_sampler);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_private void
owl_vk_image_sampler_deinit (struct owl_vk_image *image,
                             struct owl_vk_context const *ctx) {
  vkDestroySampler (ctx->vk_device, image->vk_sampler, NULL);
}

owl_private enum owl_code
owl_vk_image_upload (struct owl_vk_image *image,
                     struct owl_vk_context const *ctx,
                     struct owl_vk_image_load const *load) {
  VkBufferImageCopy copy;
  struct owl_vk_im_command_buffer cmd;

  enum owl_code code = OWL_SUCCESS;

  code = owl_vk_im_command_buffer_begin (&cmd, ctx);
  if (OWL_SUCCESS != code)
    return code;

  owl_vk_image_transition (image, &cmd, load->mips, 1,
                           VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  copy.bufferOffset = 0;
  copy.bufferRowLength = 0;
  copy.bufferImageHeight = 0;
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.mipLevel = 0;
  copy.imageSubresource.baseArrayLayer = 0;
  copy.imageSubresource.layerCount = 1;
  copy.imageOffset.x = 0;
  copy.imageOffset.y = 0;
  copy.imageOffset.z = 0;
  copy.imageExtent.width = load->width;
  copy.imageExtent.height = load->height;
  copy.imageExtent.depth = 1;

  vkCmdCopyBufferToImage (cmd.vk_command_buffer, load->allocation.vk_buffer,
                          image->vk_image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  owl_vk_image_generate_mips (image, &cmd, load->width, load->height,
                              load->mips);

  code = owl_vk_im_command_buffer_end (&cmd, ctx);
  if (OWL_SUCCESS != code)
    return code;

  return OWL_SUCCESS;
}

owl_private enum owl_code
owl_vk_image_set_init (struct owl_vk_image *image,
                       struct owl_vk_context const *ctx) {
  VkDescriptorSetAllocateInfo info;

  VkResult vk_result = VK_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.pNext = NULL;
  info.descriptorPool = ctx->vk_set_pool;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &ctx->vk_frag_image_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &info, &image->vk_set);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_image_set_deinit (struct owl_vk_image *image,
                         struct owl_vk_context const *ctx) {
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1, &image->vk_set);
}

owl_private void
owl_vk_image_set_write (struct owl_vk_image *image,
                        struct owl_vk_context const *ctx) {
  VkDescriptorImageInfo descriptors[2];
  VkWriteDescriptorSet writes[2];

  descriptors[0].sampler = image->vk_sampler;
  descriptors[0].imageView = NULL;
  descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  descriptors[1].sampler = VK_NULL_HANDLE;
  descriptors[1].imageView = image->vk_image_view;
  descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = NULL;
  writes[0].dstSet = image->vk_set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[0].pImageInfo = &descriptors[0];
  writes[0].pBufferInfo = NULL;
  writes[0].pTexelBufferView = NULL;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].pNext = NULL;
  writes[1].dstSet = image->vk_set;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &descriptors[1];
  writes[1].pBufferInfo = NULL;
  writes[1].pTexelBufferView = NULL;

  vkUpdateDescriptorSets (ctx->vk_device, owl_array_size (writes), writes, 0,
                          NULL);
}

owl_public enum owl_code
owl_vk_image_init (struct owl_vk_image *image,
                   struct owl_vk_context const *ctx,
                   struct owl_vk_stage_heap *heap,
                   struct owl_vk_image_desc const *desc) {
  struct owl_vk_image_load load;

  enum owl_code code;

  code = owl_vk_image_load_init (&load, ctx, heap, desc);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_image_image_init (image, ctx, &load);
  if (OWL_SUCCESS != code)
    goto error_image_load_deinit;

  code = owl_vk_image_memory_init (image, ctx);
  if (OWL_SUCCESS != code)
    goto error_image_deinit;

  code = owl_vk_image_image_view_init (image, ctx, &load);
  if (OWL_SUCCESS != code)
    goto error_memory_deinit;

  code = owl_vk_image_sampler_init (image, ctx, desc, &load);
  if (OWL_SUCCESS != code)
    goto error_image_view_deinit;

  code = owl_vk_image_set_init (image, ctx);
  if (OWL_SUCCESS != code)
    goto error_sampler_deinit;

  code = owl_vk_image_upload (image, ctx, &load);
  if (OWL_SUCCESS != code)
    goto error_set_deinit;

  owl_vk_image_set_write (image, ctx);
  owl_vk_image_load_deinit (&load, ctx, heap);

  goto out;

error_set_deinit:
  owl_vk_image_set_deinit (image, ctx);

error_sampler_deinit:
  owl_vk_image_sampler_deinit (image, ctx);

error_image_view_deinit:
  owl_vk_image_image_view_deinit (image, ctx);

error_memory_deinit:
  owl_vk_image_memory_deinit (image, ctx);

error_image_deinit:
  owl_vk_image_image_deinit (image, ctx);

error_image_load_deinit:
  owl_vk_image_load_deinit (&load, ctx, heap);

out:
  return code;
}

owl_public void
owl_vk_image_deinit (struct owl_vk_image *image,
                     struct owl_vk_context const *ctx) {
  owl_vk_context_device_wait (ctx);

  owl_vk_image_set_deinit (image, ctx);
  owl_vk_image_sampler_deinit (image, ctx);
  owl_vk_image_image_view_deinit (image, ctx);
  owl_vk_image_memory_deinit (image, ctx);
  owl_vk_image_image_deinit (image, ctx);
}

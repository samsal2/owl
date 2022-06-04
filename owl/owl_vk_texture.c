#include "owl_vk_texture.h"

#include "owl_internal.h"
#include "owl_vk_misc.h"
#include "owl_vk_renderer.h"
#include "owl_vk_upload.h"
#include "stb_image.h"

#include <math.h>

owl_public VkFormat
owl_vk_pixel_format_as_vk_format(enum owl_pixel_format format)
{
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

owl_public uint64_t
owl_vk_pixel_format_size(enum owl_pixel_format format)
{
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof(uint8_t);

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(uint8_t);
  }
}

owl_public uint32_t
owl_vk_texture_calc_mips(int32_t w, int32_t h)
{
  return (uint32_t)(floor(log2(owl_max(w, h))) + 1);
}

owl_private void
owl_vk_texture_transition(struct owl_vk_texture *texture,
                          struct owl_vk_renderer *vk, VkImageLayout dst)
{
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst_Stage = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = texture->layout;
  barrier.newLayout = dst;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture->image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = texture->mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = texture->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_Stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == texture->layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_Stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_Stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    owl_assert(0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    owl_assert(0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier(vk->im_command_buffer, src_stage, dst_Stage, 0, 0, NULL,
                       0, NULL, 1, &barrier);

  texture->layout = dst;
}

owl_private void
owl_vk_texture_gen_mipmaps(struct owl_vk_texture *texture,
                           struct owl_vk_renderer *vk)
{
  int32_t i;
  int32_t width;
  int32_t height;
  VkImageMemoryBarrier barrier;

  if (1 == texture->mips || 0 == texture->mips)
    return;

  width = texture->width;
  height = texture->height;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (int32_t)(texture->mips - 1); ++i) {
    VkImageBlit blit;

    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(vk->im_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                         1, &barrier);

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

    vkCmdBlitImage(vk->im_command_buffer, texture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(vk->im_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = texture->mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(vk->im_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

  texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

owl_public owl_code
owl_vk_texture_init(struct owl_vk_texture *texture, struct owl_vk_renderer *vk,
                    struct owl_vk_texture_desc *desc)
{
  int32_t width;
  int32_t height;
  uint32_t mips;
  enum owl_pixel_format pixel_format;

  uint8_t *upload_data;
  struct owl_vk_upload_allocation upload_alloc;

  VkImageCreateInfo image_info;
  VkMemoryRequirements memory_requirements;
  VkMemoryAllocateInfo memory_info;
  VkImageViewCreateInfo image_view_info;
  VkDescriptorSetAllocateInfo descriptor_set_info;
  VkDescriptorImageInfo descriptors[2];
  VkWriteDescriptorSet writes[2];
  VkBufferImageCopy copy;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  switch (desc->src) {
  case OWL_TEXTURE_SRC_DATA: {
    uint8_t *data;
    uint64_t size;

    data = desc->src_data;
    width = desc->src_data_width;
    height = desc->src_data_height;
    pixel_format = desc->src_data_pixel_format;

    size = width * height * owl_vk_pixel_format_size(pixel_format);

    upload_data = owl_vk_upload_alloc(vk, size, &upload_alloc);
    if (!upload_data)
      return OWL_ERROR_NO_UPLOAD_MEMORY;

    owl_memcpy(upload_data, data, size);

  } break;
  case OWL_TEXTURE_SRC_FILE: {
    uint8_t *data;
    int32_t channels;
    uint64_t size;

    data = stbi_load(desc->src_file_path, &width, &height, &channels,
                     STBI_rgb_alpha);
    if (!data)
      return OWL_ERROR_NOT_FOUND;

    pixel_format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;

    size = width * height * owl_vk_pixel_format_size(pixel_format);

    upload_data = owl_vk_upload_alloc(vk, size, &upload_alloc);
    if (!upload_data) {
      stbi_image_free(data);
      return OWL_ERROR_NO_UPLOAD_MEMORY;
    }

    owl_memcpy(upload_data, data, size);

    stbi_image_free(data);

  } break;
  }

  mips = owl_vk_texture_calc_mips(width, height);

  texture->width = width;
  texture->height = height;
  texture->mips = mips;
  texture->layers = 1;
  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.flags = 0;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = owl_vk_pixel_format_as_vk_format(pixel_format);
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = mips;
  image_info.arrayLayers = 1;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.initialLayout = texture->layout;

  vk_result = vkCreateImage(vk->device, &image_info, NULL, &texture->image);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_upload;
  }

  vkGetImageMemoryRequirements(vk->device, texture->image,
                               &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex =
      owl_vk_find_memory_type(vk, memory_requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(vk->device, &memory_info, NULL,
                               &texture->memory);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_image;
  }

  vk_result = vkBindImageMemory(vk->device, texture->image, texture->memory,
                                0);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = texture->image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = owl_vk_pixel_format_as_vk_format(pixel_format);
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = mips;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  vk_result = vkCreateImageView(vk->device, &image_view_info, NULL,
                                &texture->image_view);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_info.pNext = NULL;
  descriptor_set_info.descriptorPool = vk->descriptor_pool;
  descriptor_set_info.descriptorSetCount = 1;
  descriptor_set_info.pSetLayouts = &vk->image_fragment_descriptor_set_layout;

  vk_result = vkAllocateDescriptorSets(vk->device, &descriptor_set_info,
                                       &texture->descriptor_set);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_image_view;
  }

  code = owl_vk_begin_im_command_buffer(vk);
  if (code)
    goto error_free_descriptor_sets;

  owl_vk_texture_transition(texture, vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
  copy.imageExtent.width = (uint32_t)width;
  copy.imageExtent.height = (uint32_t)height;
  copy.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(vk->im_command_buffer, upload_alloc.buffer,
                         texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         1, &copy);

  owl_vk_texture_gen_mipmaps(texture, vk);

  code = owl_vk_end_im_command_buffer(vk);
  if (code)
    goto error_free_descriptor_sets;

  descriptors[0].sampler = vk->linear_sampler;
  descriptors[0].imageView = NULL;
  descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  descriptors[1].sampler = VK_NULL_HANDLE;
  descriptors[1].imageView = texture->image_view;
  descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = NULL;
  writes[0].dstSet = texture->descriptor_set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[0].pImageInfo = &descriptors[0];
  writes[0].pBufferInfo = NULL;
  writes[0].pTexelBufferView = NULL;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].pNext = NULL;
  writes[1].dstSet = texture->descriptor_set;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &descriptors[1];
  writes[1].pBufferInfo = NULL;
  writes[1].pTexelBufferView = NULL;

  vkUpdateDescriptorSets(vk->device, owl_array_size(writes), writes, 0, NULL);

  owl_vk_upload_free(vk, upload_data);

  goto out;

error_free_descriptor_sets:
  vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                       &texture->descriptor_set);

error_destroy_image_view:
  vkDestroyImageView(vk->device, texture->image_view, NULL);

error_free_memory:
  vkFreeMemory(vk->device, texture->memory, NULL);

error_destroy_image:
  vkDestroyImage(vk->device, texture->image, NULL);

error_free_upload:
  owl_vk_upload_free(vk, upload_data);

out:
  return code;
}

owl_public void
owl_vk_texture_deinit(struct owl_vk_texture *texture,
                      struct owl_vk_renderer *vk)
{
  vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                       &texture->descriptor_set);
  vkDestroyImageView(vk->device, texture->image_view, NULL);
  vkFreeMemory(vk->device, texture->memory, NULL);
  vkDestroyImage(vk->device, texture->image, NULL);
}

#include "texture.h"

#include "internal.h"
#include "renderer.h"

#include <math.h>
#include <stb/stb_image.h>

owl_u32 owl_calc_mips(owl_u32 width, owl_u32 height) {
  return (owl_u32)(floor(log2(OWL_MAX(width, height))) + 1);
}

VkFormat owl_as_vk_format_(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

OWL_INTERNAL VkDeviceSize owl_sizeof_format_(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return sizeof(owl_u8);

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(owl_u8);
  }
}

enum owl_code
owl_vk_image_transition(VkCommandBuffer cmd,
                        struct owl_vk_image_transition_info const *info) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;
  enum owl_code code = OWL_SUCCESS;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = info->from;
  barrier.newLayout = info->to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = info->image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = info->mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = info->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == info->from &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->to) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->from &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info->to) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == info->from &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == info->to) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == info->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  vkCmdPipelineBarrier(cmd, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);

end:
  return code;
}

enum owl_code
owl_vk_image_generate_mips(VkCommandBuffer cmd,
                           struct owl_vk_image_mip_info const *info) {
  owl_u32 i;
  owl_i32 width;
  owl_i32 height;
  VkImageMemoryBarrier barrier;
  enum owl_code code = OWL_SUCCESS;

  if (1 == info->mips || 0 == info->mips)
    goto end;

  width = info->width;
  height = info->height;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = info->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (info->mips - 1); ++i) {
    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &barrier);
    {
      VkImageBlit blit;
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

      vkCmdBlitImage(cmd, info->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     info->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                     &blit, VK_FILTER_LINEAR);
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = info->mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
end:
  return code;
}

OWL_INTERNAL VkFilter owl_as_vk_filter_(enum owl_sampler_filter filter) {
  switch (filter) {
  case OWL_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

OWL_INTERNAL VkSamplerMipmapMode
owl_as_vk_mipmap_mode_(enum owl_sampler_mip_mode mode) {
  switch (mode) {
  case OWL_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

OWL_INTERNAL VkSamplerAddressMode
owl_as_vk_addr_mode_(enum owl_sampler_addr_mode mode) {
  switch (mode) {
  case OWL_SAMPLER_ADDR_MODE_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;

  case OWL_SAMPLER_ADDR_MODE_MIRRORED_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

  case OWL_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  case OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }
}

VkDeviceSize owl_info_required_size_(struct owl_texture_info const *info) {
  VkDeviceSize p = (VkDeviceSize)info->width * (VkDeviceSize)info->height;
  return owl_sizeof_format_(info->format) * p;
}

enum owl_code owl_texture_init_from_ref(
    struct owl_renderer *r, struct owl_texture_info const *info,
    struct owl_dynamic_buffer_reference const *ref, struct owl_texture *tex) {
  enum owl_code code = OWL_SUCCESS;
  owl_u32 mips = owl_calc_mips((owl_u32)info->width, (owl_u32)info->height);

  tex->width = (owl_u32)info->width;
  tex->height = (owl_u32)info->height;

  {
#define OWL_IMAGE_USAGE                                                        \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |          \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_as_vk_format_(info->format);
    image.extent.width = (owl_u32)info->width;
    image.extent.height = (owl_u32)info->height;
    image.extent.depth = 1;
    image.mipLevels = mips;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = OWL_IMAGE_USAGE;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_CHECK(vkCreateImage(r->device, &image, NULL, &tex->image));

#undef OWL_IMAGE_USAGE
  }

  /* init memory */
  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetImageMemoryRequirements(r->device, tex->image, &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_CHECK(vkAllocateMemory(r->device, &memory, NULL, &tex->memory));
    OWL_CHECK(vkBindImageMemory(r->device, tex->image, tex->memory, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = tex->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_as_vk_format_(info->format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_CHECK(vkCreateImageView(r->device, &view, NULL, &tex->view));
  }

  /* init the descriptor set */
  {
    VkDescriptorSetAllocateInfo set;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = r->common_set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &r->texture_set_layout;

    OWL_CHECK(vkAllocateDescriptorSets(r->device, &set, &tex->set));
  }

  /* stage, mips and layout */
  {
    VkCommandBuffer cmd;
    owl_renderer_alloc_single_use_cmd_buffer(r, &cmd);

    {
      struct owl_vk_image_transition_info transition;

      transition.mips = mips;
      transition.layers = 1;
      transition.from = VK_IMAGE_LAYOUT_UNDEFINED;
      transition.to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      transition.image = tex->image;

      owl_vk_image_transition(cmd, &transition);
    }

    {
      VkBufferImageCopy copy;
      copy.bufferOffset = ref->offset;
      copy.bufferRowLength = 0;
      copy.bufferImageHeight = 0;
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.mipLevel = 0;
      copy.imageSubresource.baseArrayLayer = 0;
      copy.imageSubresource.layerCount = 1;
      copy.imageOffset.x = 0;
      copy.imageOffset.y = 0;
      copy.imageOffset.z = 0;
      copy.imageExtent.width = (owl_u32)info->width;
      copy.imageExtent.height = (owl_u32)info->height;
      copy.imageExtent.depth = 1;
      vkCmdCopyBufferToImage(cmd, ref->buffer, tex->image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    {
      struct owl_vk_image_mip_info mip;

      mip.width = info->width;
      mip.height = info->height;
      mip.mips = mips;
      mip.image = tex->image;

      owl_vk_image_generate_mips(cmd, &mip);
    }

    owl_renderer_free_single_use_cmd_buffer(r, cmd);
  }

  {
#define OWL_MAX_ANISOTROPY 16

    VkSamplerCreateInfo sampler;
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext = NULL;
    sampler.flags = 0;
    sampler.magFilter = owl_as_vk_filter_(info->mag_filter);
    sampler.minFilter = owl_as_vk_filter_(info->min_filter);
    sampler.mipmapMode = owl_as_vk_mipmap_mode_(info->mip_mode);
    sampler.addressModeU = owl_as_vk_addr_mode_(info->wrap_u);
    sampler.addressModeV = owl_as_vk_addr_mode_(info->wrap_v);
    sampler.addressModeW = owl_as_vk_addr_mode_(info->wrap_w);
    sampler.mipLodBias = 0.0F;
    sampler.anisotropyEnable = VK_TRUE;
    sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
    sampler.compareEnable = VK_FALSE;
    sampler.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler.minLod = 0.0F;
    sampler.maxLod = mips; /* VK_LOD_CLAMP_NONE */
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;

    OWL_CHECK(vkCreateSampler(r->device, &sampler, NULL, &tex->sampler));

#undef OWL_MAX_ANISOTROPY
  }

  {
    VkDescriptorImageInfo images[2];
    VkWriteDescriptorSet writes[2];

    images[0].sampler = tex->sampler;
    images[0].imageView = VK_NULL_HANDLE;
    images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    images[1].sampler = VK_NULL_HANDLE;
    images[1].imageView = tex->view;
    images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = tex->set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &images[0];
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = tex->set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &images[1];
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0, NULL);
  }

  owl_renderer_clear_dynamic_offset(r);

  return code;
}

enum owl_code owl_texture_init_from_data(struct owl_renderer *r,
                                         struct owl_texture_info const *info,
                                         owl_byte const *data,
                                         struct owl_texture *tex) {
  owl_byte *stage;
  VkDeviceSize size;
  struct owl_dynamic_buffer_reference ref;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_buffer_clear(r));

  size = owl_info_required_size_(info);

  if (!(stage = owl_renderer_dynamic_buffer_alloc(r, size, &ref))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  OWL_MEMCPY(stage, data, size);

  code = owl_texture_init_from_ref(r, info, &ref, tex);

end:
  return code;
}

enum owl_code owl_texture_init_from_file(struct owl_renderer *r,
                                         struct owl_texture_info *info,
                                         char const *path,
                                         struct owl_texture *tex) {
  owl_byte *data;
  enum owl_code code = OWL_SUCCESS;

  if (!(data = owl_texture_data_from_file(path, info))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  code = owl_texture_init_from_data(r, info, data, tex);

  stbi_image_free(data);

end:
  return code;
}

void owl_texture_deinit(struct owl_renderer const *r, struct owl_texture *tex) {
  OWL_CHECK(vkDeviceWaitIdle(r->device));

  vkDestroySampler(r->device, tex->sampler, NULL);
  vkFreeDescriptorSets(r->device, r->common_set_pool, 1, &tex->set);
  vkFreeMemory(r->device, tex->memory, NULL);
  vkDestroyImageView(r->device, tex->view, NULL);
  vkDestroyImage(r->device, tex->image, NULL);
}

enum owl_code owl_texture_create_from_data(struct owl_renderer *r,
                                           struct owl_texture_info const *info,
                                           owl_byte const *data,
                                           struct owl_texture **tex) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*tex = OWL_MALLOC(sizeof(**tex)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  code = owl_texture_init_from_data(r, info, data, *tex);

  if (OWL_SUCCESS != code)
    OWL_FREE(*tex);

end:
  return code;
}

enum owl_code owl_texture_create_from_file(struct owl_renderer *r,
                                           struct owl_texture_info *info,
                                           char const *path,
                                           struct owl_texture **tex) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*tex = OWL_MALLOC(sizeof(**tex)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  code = owl_texture_init_from_file(r, info, path, *tex);

  if (OWL_SUCCESS != code)
    OWL_FREE(*tex);

end:
  return code;
}

void owl_texture_destroy(struct owl_renderer const *r,
                         struct owl_texture *tex) {
  owl_texture_deinit(r, tex);
  OWL_FREE(tex);
}

owl_byte *owl_texture_data_from_file(char const *path,
                                     struct owl_texture_info *info) {

  int channels;
  info->format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;
  return stbi_load(path, &info->width, &info->height, &channels,
                   STBI_rgb_alpha);
}

void owl_texture_free_data_from_file(owl_byte *data) { stbi_image_free(data); }

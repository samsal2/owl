#include "image.h"

#include "internal.h"
#include "renderer.h"
#include "types.h"
#include "vulkan/vulkan_core.h"

#include <math.h>
#include <stb/stb_image.h>

#define OWL_MAX_ANISOTROPY 16.0F

OWL_INTERNAL owl_u32 owl_calc_mips_(owl_u32 width, owl_u32 height) {
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

OWL_INTERNAL VkFilter owl_as_vk_filter_(enum owl_sampler_filter filter) {
  switch (filter) {
  case OWL_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

OWL_INTERNAL VkSamplerMipmapMode
owl_as_vk_mip_mode_(enum owl_sampler_mip_mode mode) {
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

OWL_INTERNAL VkDeviceSize owl_pixel_format_size_(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof(owl_u8);

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(owl_u8);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_find_image_manager_slot_(struct owl_renderer *r,
                                      struct owl_image *i) {
  int j;
  enum owl_code code = OWL_SUCCESS;

  for (j = 0; j < OWL_RENDERER_IMAGE_MANAGER_SLOTS_SIZE; ++j) {
    if (!r->image_manager_slots[j]) {
      i->slot = j;
      r->image_manager_slots[j] = 1;
      goto end;
    }
  }

  code = OWL_ERROR_UNKNOWN;
  i->slot = OWL_RENDERER_IMAGE_MANAGER_SLOTS_SIZE;

end:
  return code;
}

enum owl_code owl_image_init(struct owl_renderer *r,
                             struct owl_image_init_info const *iii,
                             struct owl_image *i) {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  enum owl_pixel_format format;
  struct owl_dynamic_heap_reference dhr;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(
      owl_renderer_is_dynamic_heap_offset_clear(r) ||
      (OWL_IMAGE_SOURCE_TYPE_DYNAMIC_HEAP_REFERENCE == iii->source_type));

  if (!r || !iii || !i) {
    code = OWL_ERROR_BAD_PTR;
    goto end;
  }

  switch (iii->source_type) {
  case OWL_IMAGE_SOURCE_TYPE_FILE: {
    struct owl_image_info ii;
    VkDeviceSize size;
    owl_byte *data;

    if (OWL_SUCCESS != (code = owl_image_load_info(iii->path, &ii)))
      goto end;

    format = ii.format;
    width = (owl_u32)ii.width;
    height = (owl_u32)ii.height;
    size = owl_pixel_format_size_(ii.format) * width * height;

    if (!(data = owl_renderer_dynamic_heap_alloc(r, size, &dhr))) {
      owl_image_free_info(&ii);
      code = OWL_ERROR_UNKNOWN;
      goto end;
    }

    OWL_MEMCPY(data, ii.data, size);

  } break;

  case OWL_IMAGE_SOURCE_TYPE_DATA: {
    VkDeviceSize size;
    owl_byte *data;

    format = iii->format;
    width = (owl_u32)iii->width;
    height = (owl_u32)iii->height;
    size = owl_pixel_format_size_(iii->format) * width * height;

    if (!(data = owl_renderer_dynamic_heap_alloc(r, size, &dhr))) {
      code = OWL_ERROR_UNKNOWN;
      goto end;
    }

    OWL_MEMCPY(data, iii->data, size);
  } break;

  case OWL_IMAGE_SOURCE_TYPE_DYNAMIC_HEAP_REFERENCE: {
    format = iii->format;
    width = (owl_u32)iii->width;
    height = (owl_u32)iii->height;
    /*dhr = *iii->reference; this works, but ill rather be explicit*/
    dhr.offset32 = iii->reference->offset32;
    dhr.offset = iii->reference->offset;
    dhr.buffer = iii->reference->buffer;
    dhr.pvm_set = iii->reference->pvm_set;
    dhr.scene_set = iii->reference->scene_set;
  } break;
  }

  mips = owl_calc_mips_(width, height);

  if (OWL_SUCCESS != (code = owl_renderer_find_image_manager_slot_(r, i)))
    goto end;

  {
    VkImageCreateInfo image;

    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_as_vk_format_(format);
    image.extent.width = width;
    image.extent.height = height;
    image.extent.depth = 1;
    image.mipLevels = mips;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(r->device, &image, NULL,
                               &r->image_manager_images[i->slot]));
  }

  {
    VkMemoryAllocateInfo memory;
    VkMemoryRequirements requirements;

    vkGetImageMemoryRequirements(r->device, r->image_manager_images[i->slot],
                                 &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(vkAllocateMemory(r->device, &memory, NULL,
                                  &r->image_manager_memories[i->slot]));
    OWL_VK_CHECK(vkBindImageMemory(r->device, r->image_manager_images[i->slot],
                                   r->image_manager_memories[i->slot], 0));
  }

  {
    VkImageViewCreateInfo view;

    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = r->image_manager_images[i->slot];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_as_vk_format_(format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(r->device, &view, NULL,
                                   &r->image_manager_views[i->slot]));
  }

  {
    VkDescriptorSetLayout layout;
    VkDescriptorSetAllocateInfo set;

    layout = r->image_set_layout;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = r->common_set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &layout;

    OWL_VK_CHECK(vkAllocateDescriptorSets(r->device, &set,
                                          &r->image_manager_sets[i->slot]));
  }

  {
    struct owl_single_use_command_buffer sucb;
    owl_renderer_init_single_use_command_buffer(r, &sucb);

    {
      struct owl_vk_image_transition_info iti;

      iti.mips = mips;
      iti.layers = 1;
      iti.from = VK_IMAGE_LAYOUT_UNDEFINED;
      iti.to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      iti.image = r->image_manager_images[i->slot];
      iti.command_buffer = sucb.command_buffer;

      owl_vk_image_transition(&iti);
    }

    {
      VkBufferImageCopy copy;

      copy.bufferOffset = dhr.offset;
      copy.bufferRowLength = 0;
      copy.bufferImageHeight = 0;
      copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copy.imageSubresource.mipLevel = 0;
      copy.imageSubresource.baseArrayLayer = 0;
      copy.imageSubresource.layerCount = 1;
      copy.imageOffset.x = 0;
      copy.imageOffset.y = 0;
      copy.imageOffset.z = 0;
      copy.imageExtent.width = width;
      copy.imageExtent.height = height;
      copy.imageExtent.depth = 1;

      vkCmdCopyBufferToImage(sucb.command_buffer, dhr.buffer,
                             r->image_manager_images[i->slot],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    {
      struct owl_vk_image_mip_info imi;

      imi.width = (owl_i32)width;
      imi.height = (owl_i32)height;
      imi.mips = mips;
      imi.image = r->image_manager_images[i->slot];
      imi.command_buffer = sucb.command_buffer;

      owl_vk_image_generate_mips(&imi);
    }

    owl_renderer_deinit_single_use_command_buffer(r, &sucb);
  }

  {
    VkSamplerCreateInfo sampler;

    if (iii->use_default_sampler) {
      sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler.pNext = NULL;
      sampler.flags = 0;
      sampler.magFilter = VK_FILTER_LINEAR;
      sampler.minFilter = VK_FILTER_LINEAR;
      sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler.mipLodBias = 0.0F;
      sampler.anisotropyEnable = VK_TRUE;
      sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
      sampler.compareEnable = VK_FALSE;
      sampler.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler.minLod = 0.0F;
      sampler.maxLod = mips; /* VK_LOD_CLAMP_NONE */
      sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler.unnormalizedCoordinates = VK_FALSE;
    } else {
      sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler.pNext = NULL;
      sampler.flags = 0;
      sampler.magFilter = owl_as_vk_filter_(iii->mag_filter);
      sampler.minFilter = owl_as_vk_filter_(iii->min_filter);
      sampler.mipmapMode = owl_as_vk_mip_mode_(iii->mip_mode);
      sampler.addressModeU = owl_as_vk_addr_mode_(iii->wrap_u);
      sampler.addressModeV = owl_as_vk_addr_mode_(iii->wrap_v);
      sampler.addressModeW = owl_as_vk_addr_mode_(iii->wrap_w);
      sampler.mipLodBias = 0.0F;
      sampler.anisotropyEnable = VK_TRUE;
      sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
      sampler.compareEnable = VK_FALSE;
      sampler.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler.minLod = 0.0F;
      sampler.maxLod = mips; /* VK_LOD_CLAMP_NONE */
      sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler.unnormalizedCoordinates = VK_FALSE;
    }

    OWL_VK_CHECK(vkCreateSampler(r->device, &sampler, NULL,
                                 &r->image_manager_samplers[i->slot]));
  }

  {
    VkDescriptorImageInfo image;
    VkDescriptorImageInfo sampler;
    VkWriteDescriptorSet writes[2];

    sampler.sampler = r->image_manager_samplers[i->slot];
    sampler.imageView = VK_NULL_HANDLE;
    sampler.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image.sampler = VK_NULL_HANDLE;
    image.imageView = r->image_manager_views[i->slot];
    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = r->image_manager_sets[i->slot];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &sampler;
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = r->image_manager_sets[i->slot];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &image;
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0, NULL);
  }

  owl_renderer_clear_dynamic_heap_offset(r);
end:
  return code;
}

void owl_image_deinit(struct owl_renderer *r, struct owl_image *i) {
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));
  OWL_ASSERT(r->image_manager_slots[i->slot]);

  r->image_manager_slots[i->slot] = 0;

  vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                       &r->image_manager_sets[i->slot]);
  vkDestroySampler(r->device, r->image_manager_samplers[i->slot], NULL);
  vkDestroyImageView(r->device, r->image_manager_views[i->slot], NULL);
  vkFreeMemory(r->device, r->image_manager_memories[i->slot], NULL);
  vkDestroyImage(r->device, r->image_manager_images[i->slot], NULL);
}

enum owl_code owl_image_load_info(char const *path, struct owl_image_info *ii) {
  int ch;
  enum owl_code code = OWL_SUCCESS;

  ii->format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;
  ii->data = stbi_load(path, &ii->width, &ii->height, &ch, STBI_rgb_alpha);

  return code;
}

void owl_image_free_info(struct owl_image_info *ii) {
  stbi_image_free(ii->data);
}

enum owl_code
owl_vk_image_transition(struct owl_vk_image_transition_info const *iti) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;
  enum owl_code code = OWL_SUCCESS;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = iti->from;
  barrier.newLayout = iti->to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = iti->image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = iti->mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = iti->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == iti->from &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == iti->to) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == iti->from &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == iti->to) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == iti->from &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == iti->to) {
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

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == iti->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == iti->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == iti->to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  vkCmdPipelineBarrier(iti->command_buffer, src, dst, 0, 0, NULL, 0, NULL, 1,
                       &barrier);

end:
  return code;
}

enum owl_code
owl_vk_image_generate_mips(struct owl_vk_image_mip_info const *imi) {
  owl_u32 i;
  owl_i32 width;
  owl_i32 height;
  VkImageMemoryBarrier barrier;
  enum owl_code code = OWL_SUCCESS;

  if (1 == imi->mips || 0 == imi->mips)
    goto end;

  width = imi->width;
  height = imi->height;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = imi->image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (imi->mips - 1); ++i) {
    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(imi->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

      vkCmdBlitImage(imi->command_buffer, imi->image,
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, imi->image,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                     VK_FILTER_LINEAR);
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(imi->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = imi->mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(imi->command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
end:
  return code;
}
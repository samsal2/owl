#include "img.h"

#include "internal.h"
#include "renderer.h"

#include <math.h>
#include <stb/stb_image.h>

VkCommandBuffer owl_alloc_vk_cmd_buf(struct owl_vk_renderer const *renderer) {
  VkCommandBuffer cmd;

  {
    VkCommandBufferAllocateInfo buf;

    buf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buf.pNext = NULL;
    buf.commandPool = renderer->transient_cmd_pool;
    buf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buf.commandBufferCount = 1;

    OWL_VK_CHECK(vkAllocateCommandBuffers(renderer->device, &buf, &cmd));
  }

  {
    VkCommandBufferBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = NULL;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin.pInheritanceInfo = NULL;

    OWL_VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
  }

  return cmd;
}

void owl_free_vk_cmd_buf(struct owl_vk_renderer *renderer,
                         VkCommandBuffer cmd) {
  VkSubmitInfo submit;

  OWL_VK_CHECK(vkEndCommandBuffer(cmd));

  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = NULL;
  submit.waitSemaphoreCount = 0;
  submit.pWaitSemaphores = NULL;
  submit.pWaitDstStageMask = NULL;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  submit.signalSemaphoreCount = 0;
  submit.pSignalSemaphores = NULL;

  OWL_VK_CHECK(vkQueueSubmit(renderer->graphics_queue, 1, &submit, NULL));
  OWL_VK_CHECK(vkQueueWaitIdle(renderer->graphics_queue));

  vkFreeCommandBuffers(renderer->device, renderer->transient_cmd_pool, 1, &cmd);
}

OWL_INTERNAL uint32_t owl_calc_mips_(int width, int height) {
  return (uint32_t)(floor(log2(OWL_MAX(width, height))) + 1);
}

OWL_INTERNAL VkFormat owl_as_vk_format_(enum owl_pixel_format format) {
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

void owl_transition_vk_img_layout(VkCommandBuffer cmd, VkImageLayout from,
                                  VkImageLayout to, owl_u32 mip,
                                  VkImage image) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = from;
  barrier.newLayout = to;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mip;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  if (VK_IMAGE_LAYOUT_UNDEFINED == from &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == to) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == from &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == to) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == from &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == to) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == to) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier(cmd, src, dst, 0, 0, NULL, 0, NULL, 1, &barrier);
}

void owl_generate_vk_img_mips(VkCommandBuffer cmd, int width, int height,
                              owl_u32 mips, VkImage image) {
  owl_u32 i;
  VkImageMemoryBarrier barrier;

  if (mips == 1 || mips == 0)
    return;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (mips - 1); ++i) {
    VkImageBlit blit;

    barrier.subresourceRange.baseMipLevel = i;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &barrier);

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

    vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = mips - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);
}

VkFilter owl_as_vk_filter_(enum owl_sampler_filter filter) {
  switch (filter) {
  case OWL_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

VkSamplerMipmapMode owl_as_vk_mipmap_mode_(enum owl_sampler_mip_mode mode) {
  switch (mode) {
  case OWL_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

VkSamplerAddressMode owl_as_vk_addr_mode_(enum owl_sampler_addr_mode mode) {
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

OWL_INTERNAL VkDeviceSize
owl_attr_required_size_(struct owl_img_desc const *attr) {
  VkDeviceSize pixels = (unsigned)attr->width * (unsigned)attr->height;
  return owl_sizeof_format_(attr->format) * pixels;
}

enum owl_code owl_init_vk_img_from_ref(struct owl_vk_renderer *renderer,
                                       struct owl_img_desc const *desc,
                                       struct owl_dyn_buf_alloc_ref const *ref,
                                       struct owl_img *out) {
  enum owl_code code = OWL_SUCCESS;
  owl_u32 mips = owl_calc_mips_(desc->width, desc->height);

  out->width = (owl_u32)desc->width;
  out->height = (owl_u32)desc->height;

  {
#define OWL_IMAGE_USAGE                                                        \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |          \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo img;
    img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img.pNext = NULL;
    img.flags = 0;
    img.imageType = VK_IMAGE_TYPE_2D;
    img.format = owl_as_vk_format_(desc->format);
    img.extent.width = (owl_u32)desc->width;
    img.extent.height = (owl_u32)desc->height;
    img.extent.depth = 1;
    img.mipLevels = mips;
    img.arrayLayers = 1;
    img.samples = VK_SAMPLE_COUNT_1_BIT;
    img.tiling = VK_IMAGE_TILING_OPTIMAL;
    img.usage = OWL_IMAGE_USAGE;
    img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img.queueFamilyIndexCount = 0;
    img.pQueueFamilyIndices = NULL;
    img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(renderer->device, &img, NULL, &out->img));

#undef OWL_IMAGE_USAGE
  }

  /* init memory */
  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetImageMemoryRequirements(renderer->device, out->img, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_GPU_ONLY);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &mem, NULL, &out->mem));

    OWL_VK_CHECK(vkBindImageMemory(renderer->device, out->img, out->mem, 0));
  }

  {
    VkImageViewCreateInfo view;

    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = out->img;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_as_vk_format_(desc->format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view, NULL, &out->view));
  }

  /* init the descriptor set */
  {
    VkDescriptorSetAllocateInfo set;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = renderer->set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &renderer->tex_set_layout;

    OWL_VK_CHECK(vkAllocateDescriptorSets(renderer->device, &set, &out->set));
  }

  /* stage, mips and layout */
  {
    VkBufferImageCopy copy;
    VkCommandBuffer cmd;

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
    copy.imageExtent.width = (owl_u32)desc->width;
    copy.imageExtent.height = (owl_u32)desc->height;
    copy.imageExtent.depth = 1;

    cmd = owl_alloc_vk_cmd_buf(renderer);

    owl_transition_vk_img_layout(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mips,
                                 out->img);

    vkCmdCopyBufferToImage(cmd, ref->buf, out->img,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    owl_generate_vk_img_mips(cmd, desc->width, desc->height, mips, out->img);

    owl_free_vk_cmd_buf(renderer, cmd);
  }

  {
#define OWL_MAX_ANISOTROPY 16

    VkSamplerCreateInfo sampler;
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.pNext = NULL;
    sampler.flags = 0;
    sampler.magFilter = owl_as_vk_filter_(desc->mag_filter);
    sampler.minFilter = owl_as_vk_filter_(desc->min_filter);
    sampler.mipmapMode = owl_as_vk_mipmap_mode_(desc->mip_mode);
    sampler.addressModeU = owl_as_vk_addr_mode_(desc->wrap_u);
    sampler.addressModeV = owl_as_vk_addr_mode_(desc->wrap_v);
    sampler.addressModeW = owl_as_vk_addr_mode_(desc->wrap_w);
    sampler.mipLodBias = 0.0F;
    sampler.anisotropyEnable = VK_TRUE;
    sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
    sampler.compareEnable = VK_FALSE;
    sampler.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler.minLod = 0.0F;
    sampler.maxLod = mips; /* VK_LOD_CLAMP_NONE */
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;

    OWL_VK_CHECK(
        vkCreateSampler(renderer->device, &sampler, NULL, &out->sampler));

#undef OWL_MAX_ANISOTROPY
  }

  {
    VkDescriptorImageInfo image;
    VkWriteDescriptorSet write;

    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image.imageView = out->view;
    image.sampler = out->sampler;

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = out->set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
  }

  owl_flush_dyn_buf(renderer);

  return code;
}

enum owl_code owl_init_vk_img(struct owl_vk_renderer *renderer,
                              struct owl_img_desc const *attr,
                              owl_byte const *data, struct owl_img *img) {
  owl_byte *stage;
  VkDeviceSize size;
  struct owl_dyn_buf_alloc_ref ref;

  OWL_ASSERT(owl_is_dyn_buf_flushed(renderer));

  size = owl_attr_required_size_(attr);

  if (!(stage = owl_dyn_buf_alloc(renderer, size, &ref)))
    return OWL_ERROR_BAD_ALLOC;

  OWL_MEMCPY(stage, data, size);

  return owl_init_vk_img_from_ref(renderer, attr, &ref, img);
}

enum owl_code owl_init_vk_img_from_file(struct owl_vk_renderer *renderer,
                                        struct owl_img_desc *attr,
                                        char const *path, struct owl_img *img) {
  int channels;
  int width;
  int height;
  owl_byte *data;
  enum owl_code code;

  if (!(data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha)))
    return OWL_ERROR_BAD_ALLOC;

  /* FIXME(samuel): HACK, for now only support rgba */
  attr->format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;
  attr->width = width;
  attr->height = height;

  code = owl_init_vk_img(renderer, attr, data, img);

  stbi_image_free(data);

  return code;
}

void owl_deinit_vk_img(struct owl_vk_renderer const *renderer,
                       struct owl_img *img) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  vkDestroySampler(renderer->device, img->sampler, NULL);
  vkFreeDescriptorSets(renderer->device, renderer->set_pool, 1, &img->set);
  vkFreeMemory(renderer->device, img->mem, NULL);
  vkDestroyImageView(renderer->device, img->view, NULL);
  vkDestroyImage(renderer->device, img->img, NULL);
}

enum owl_code owl_create_img(struct owl_vk_renderer *renderer,
                             struct owl_img_desc const *attr,
                             owl_byte const *data, struct owl_img **img) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*img = OWL_MALLOC(sizeof(**img))))
    return OWL_ERROR_BAD_ALLOC;

  if (OWL_SUCCESS != (code = owl_init_vk_img(renderer, attr, data, *img)))
    OWL_FREE(*img);

  return code;
}

enum owl_code owl_create_img_from_file(struct owl_vk_renderer *renderer,
                                       struct owl_img_desc *attr,
                                       char const *path, struct owl_img **img) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*img = OWL_MALLOC(sizeof(**img))))
    return OWL_ERROR_BAD_ALLOC;

  code = owl_init_vk_img_from_file(renderer, attr, path, *img);

  if (OWL_SUCCESS != code)
    OWL_FREE(*img);

  return code;
}

void owl_destroy_img(struct owl_vk_renderer const *renderer,
                     struct owl_img *img) {
  owl_deinit_vk_img(renderer, img);
  OWL_FREE(img);
}
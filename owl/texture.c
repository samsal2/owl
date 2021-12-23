#include <math.h>
#include <owl/internal.h>
#include <owl/memory.h>
#include <owl/renderer_internal.h>
#include <owl/texture.h>
#include <owl/texture_internal.h>
#include <owl/types.h>
#include <stb/stb_image.h>

OWL_INTERNAL VkCommandBuffer
owl_alloc_cmd_buf_(struct owl_renderer const *renderer) {
  VkCommandBuffer cmd;

  {
    VkCommandBufferAllocateInfo buf;

    buf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buf.pNext = NULL;
    buf.commandPool = renderer->cmd_pool;
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

OWL_INTERNAL void owl_free_cmd_buf_(struct owl_renderer *renderer,
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

  vkFreeCommandBuffers(renderer->device, renderer->cmd_pool, 1, &cmd);
}

OWL_INTERNAL uint32_t owl_calc_mips_(int width, int height) {
  return (uint32_t)(floor(log2(OWL_MAX(width, height))) + 1);
}

OWL_INTERNAL VkFormat owl_to_vk_format_(enum owl_pixel_format format) {
#define OWL_FORMAT_CASE(format)                                              \
  case OWL_PIXEL_FORMAT_##format:                                            \
    return VK_FORMAT_##format

  switch (format) {
    OWL_FORMAT_CASE(R8_UNORM);
    OWL_FORMAT_CASE(R8G8B8A8_SRGB);
  }

#undef OWL_FORMAT_CASE
}

OWL_INTERNAL OwlDeviceSize owl_sizeof_format_(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return sizeof(OwlU8);
  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(OwlU8);
  }
}

OWL_INTERNAL void owl_transition_image_layout_(VkCommandBuffer cmd,
                                               VkImageLayout from,
                                               VkImageLayout to,
                                               OtterVkMipLevels mip,
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

OWL_INTERNAL void owl_gen_mips_(VkCommandBuffer cmd, int width, int height,
                                OtterVkMipLevels mips, VkImage image) {
  OwlU32 i;
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

enum owl_code owl_init_texture(struct owl_renderer *renderer, int width,
                               int height, OwlByte const *data,
                               enum owl_pixel_format format,
                               enum owl_sampler_type sampler,
                               struct owl_vk_texture *texture) {
  enum owl_code err = OWL_SUCCESS;
  OtterVkMipLevels mips = owl_calc_mips_(width, height);

  texture->size.width = width;
  texture->size.height = height;

  {
#define OWL_IMAGE_USAGE                                                      \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |        \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_to_vk_format_(format);
    image.extent.width = (uint32_t)width;
    image.extent.height = (uint32_t)height;
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

    OWL_VK_CHECK(
        vkCreateImage(renderer->device, &image, NULL, &texture->img));

#undef OWL_IMAGE_USAGE
  }

  /* init memory */
  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetImageMemoryRequirements(renderer->device, texture->img, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_vk_find_mem_type(
        renderer, req.memoryTypeBits, OWL_VK_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &texture->mem));

    OWL_VK_CHECK(
        vkBindImageMemory(renderer->device, texture->img, texture->mem, 0));
  }

  {
    VkImageViewCreateInfo view;

    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = texture->img;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_to_vk_format_(format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(
        vkCreateImageView(renderer->device, &view, NULL, &texture->view));
  }

  /* init the descriptor set */
  {
    VkDescriptorSetAllocateInfo set;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = renderer->set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &renderer->tex_set_layout;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set, &texture->set));
  }

  /* stage, mips and layout */
  {
    VkBufferImageCopy copy;
    OwlByte *stage;
    OwlDeviceSize size;
    struct owl_tmp_submit_mem_ref ref;
    VkCommandBuffer cmd = owl_alloc_cmd_buf_(renderer);

    size = (unsigned)width * (unsigned)height * owl_sizeof_format_(format);

    if (!(stage = owl_alloc_tmp_submit_mem(renderer, size, &ref))) {
      err = OWL_ERROR_BAD_ALLOC;
      goto end;
    }

    OWL_MEMCPY(stage, data, size);

    copy.bufferOffset = ref.offset;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x = 0;
    copy.imageOffset.y = 0;
    copy.imageOffset.z = 0;
    copy.imageExtent.width = (OwlU32)width;
    copy.imageExtent.height = (OwlU32)height;
    copy.imageExtent.depth = 1;

    cmd = owl_alloc_cmd_buf_(renderer);

    owl_transition_image_layout_(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mips,
                                 texture->img);

    vkCmdCopyBufferToImage(cmd, ref.buf, texture->img,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    owl_gen_mips_(cmd, width, height, mips, texture->img);

    owl_free_cmd_buf_(renderer, cmd);
  }

  {
    VkDescriptorImageInfo image;
    VkWriteDescriptorSet write;

    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image.imageView = texture->view;
    image.sampler = renderer->samplers[sampler];

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = texture->set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
  }

end:
  return err;
}

enum owl_code owl_init_texture_with_ref(
    struct owl_renderer *renderer, int width, int height,
    struct owl_tmp_submit_mem_ref const *ref, enum owl_pixel_format format,
    enum owl_sampler_type sampler, struct owl_vk_texture *texture) {
  enum owl_code err = OWL_SUCCESS;
  OtterVkMipLevels mips = owl_calc_mips_(width, height);

  texture->size.width = width;
  texture->size.height = height;

  {
#define OWL_IMAGE_USAGE                                                      \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |        \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_to_vk_format_(format);
    image.extent.width = (uint32_t)width;
    image.extent.height = (uint32_t)height;
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

    OWL_VK_CHECK(
        vkCreateImage(renderer->device, &image, NULL, &texture->img));

#undef OWL_IMAGE_USAGE
  }

  /* init memory */
  {
    VkMemoryAllocateInfo mem;
    VkMemoryRequirements req;

    vkGetImageMemoryRequirements(renderer->device, texture->img, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_vk_find_mem_type(
        renderer, req.memoryTypeBits, OWL_VK_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &texture->mem));

    OWL_VK_CHECK(
        vkBindImageMemory(renderer->device, texture->img, texture->mem, 0));
  }

  {
    VkImageViewCreateInfo view;

    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = texture->img;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = owl_to_vk_format_(format);
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(
        vkCreateImageView(renderer->device, &view, NULL, &texture->view));
  }

  /* init the descriptor set */
  {
    VkDescriptorSetAllocateInfo set;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = renderer->set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &renderer->tex_set_layout;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set, &texture->set));
  }

  /* stage, mips and layout */
  {
    VkBufferImageCopy copy;
    VkCommandBuffer cmd = owl_alloc_cmd_buf_(renderer);

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
    copy.imageExtent.width = (OwlU32)width;
    copy.imageExtent.height = (OwlU32)height;
    copy.imageExtent.depth = 1;

    cmd = owl_alloc_cmd_buf_(renderer);

    owl_transition_image_layout_(cmd, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mips,
                                 texture->img);

    vkCmdCopyBufferToImage(cmd, ref->buf, texture->img,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    owl_gen_mips_(cmd, width, height, mips, texture->img);

    owl_free_cmd_buf_(renderer, cmd);
  }

  {
    VkDescriptorImageInfo image;
    VkWriteDescriptorSet write;

    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image.imageView = texture->view;
    image.sampler = renderer->samplers[sampler];

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = texture->set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
  }

  return err;
}

void owl_deinit_texture(struct owl_renderer const *renderer,
                        struct owl_vk_texture *texture) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  vkFreeDescriptorSets(renderer->device, renderer->set_pool, 1,
                       &texture->set);
  vkFreeMemory(renderer->device, texture->mem, NULL);
  vkDestroyImageView(renderer->device, texture->view, NULL);
  vkDestroyImage(renderer->device, texture->img, NULL);
}

enum owl_code
owl_init_texture_manager(struct owl_renderer const *renderer,
                         struct owl_vk_texture_manager *manager) {
  int i;

  OWL_UNUSED(renderer);

  manager->current = 0;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    manager->slots[i] = 0;

  return OWL_SUCCESS;
}

void owl_deinit_texture_manager(struct owl_renderer const *renderer,
                                struct owl_vk_texture_manager *manager) {
  int i;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (manager->slots[i])
      owl_deinit_texture(renderer, &manager->textures[i]);
}

OWL_GLOBAL int g_manager_exists = 0;
OWL_GLOBAL struct owl_vk_texture_manager g_manager;

/* check error codes */
OWL_INTERNAL void owl_ensure_manager_(struct owl_renderer const *renderer) {
  if (!g_manager_exists) {
    /* as the texture manager is static, this is redundant. but is nice to
     * have in case it changes*/
    owl_init_texture_manager(renderer, &g_manager);
    g_manager_exists = 1;
  }
}

enum owl_code owl_create_texture(struct owl_renderer *renderer, int width,
                                 int height, OwlByte const *data,
                                 enum owl_pixel_format format,
                                 enum owl_sampler_type sampler,
                                 OwlTexture *texture) {
  int i;
  enum owl_code err = OWL_SUCCESS;

  owl_ensure_manager_(renderer);

  if (OWL_INVALID_TEXTURE == g_manager.current) {
    err = OWL_ERROR_BAD_HANDLE;
    goto end;
  }

  if (OWL_SUCCESS !=
      (err = owl_init_texture(renderer, width, height, data, format, sampler,
                              &g_manager.textures[g_manager.current])))
    goto end;

  *texture = g_manager.current;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (!g_manager.slots[i])
      g_manager.current = i;

end:
  return err;
}

enum owl_code owl_create_texture_from_file(struct owl_renderer *renderer,
                                           char const *path,
                                           enum owl_sampler_type sampler,
                                           OwlTexture *texture) {
  int width;
  int height;
  int channels;
  OwlByte *data;
  enum owl_code err = OWL_SUCCESS;

  if (!(data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha))) {
    err = OWL_ERROR_BAD_INIT;
    goto end;
  }

  if (OWL_SUCCESS != (err = owl_create_texture(renderer, width, height, data,
                                               OWL_PIXEL_FORMAT_R8G8B8A8_SRGB,
                                               sampler, texture))) {
    goto end_image_free;
  }

end_image_free:
  stbi_image_free(data);

end:
  return err;
}

void owl_destroy_texture(struct owl_renderer const *renderer,
                         OwlTexture texture) {
  int i;

  owl_deinit_texture(renderer, &g_manager.textures[texture]);

  g_manager.slots[texture] = 0;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (!g_manager.slots[i])
      g_manager.current = i;
}

struct owl_vk_texture *owl_get_texture(OwlTexture texture) {
  return &g_manager.textures[texture];
}

enum owl_code owl_create_texture_with_ref(
    struct owl_renderer *renderer, int width, int height,
    struct owl_tmp_submit_mem_ref const *ref, enum owl_pixel_format format,
    enum owl_sampler_type sampler, OwlTexture *texture) {
  int i;
  enum owl_code err = OWL_SUCCESS;

  owl_ensure_manager_(renderer);

  if (OWL_INVALID_TEXTURE == g_manager.current) {
    err = OWL_ERROR_BAD_HANDLE;
    goto end;
  }

  if (OWL_SUCCESS != (err = owl_init_texture_with_ref(
                          renderer, width, height, ref, format, sampler,
                          &g_manager.textures[g_manager.current])))
    goto end;

  *texture = g_manager.current;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (!g_manager.slots[i])
      g_manager.current = i;

end:
  return err;
}

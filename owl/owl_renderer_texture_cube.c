#include "owl_renderer_texture_cube.h"

#include "owl_internal.h"
#include "owl_renderer.h"
#include "stb_image.h"

OWL_PRIVATE void
owl_renderer_texture_cube_transition(struct owl_renderer_texture_cube *texture,
    struct owl_renderer *renderer, VkImageLayout dst_layout) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = texture->layout;
  barrier.newLayout = dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture->image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == texture->layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments1");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments2");
  }

  vkCmdPipelineBarrier(renderer->immediate_command_buffer, src_stage,
      dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

  texture->layout = dst_layout;
}

OWL_PUBLIC owl_code
owl_renderer_texture_cube_init(struct owl_renderer_texture_cube *texture,
    struct owl_renderer *renderer,
    struct owl_renderer_texture_cube_desc *desc) {
  int width;
  int height;
  int chans;
  uint8_t *data = NULL;

  VkBufferImageCopy copies[6];

  uint8_t *upload_data;
  struct owl_renderer_upload_allocation upload_alloc;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;

  data = stbi_load(desc->files[0], &width, &height, &chans, STBI_rgb_alpha);
  if (!data) {
    code = OWL_ERROR_NOT_FOUND;
    goto out;
  }

  {
    VkImageCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_info.extent.width = (uint32_t)width;
    image_info.extent.height = (uint32_t)height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 6; /* 6 sides of the cube */
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &image_info, NULL,
        &texture->image);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }
  {
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetImageMemoryRequirements(renderer->device, texture->image,
        &memory_requirements);

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(renderer,
        memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
        &texture->memory);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image;
    }

    vk_result = vkBindImageMemory(renderer->device, texture->image,
        texture->memory, 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memory;
    }
  }

  {
    VkImageViewCreateInfo image_view_info;

    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.pNext = NULL;
    image_view_info.flags = 0;
    image_view_info.image = texture->image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    image_view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 6;

    vk_result = vkCreateImageView(renderer->device, &image_view_info, NULL,
        &texture->image_view);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memory;
    }
  }

  {
    VkDescriptorSetAllocateInfo descriptor_set_info;

    descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_info.pNext = NULL;
    descriptor_set_info.descriptorPool = renderer->descriptor_pool;
    descriptor_set_info.descriptorSetCount = 1;
    descriptor_set_info.pSetLayouts =
        &renderer->image_fragment_descriptor_set_layout;

    vk_result = vkAllocateDescriptorSets(renderer->device,
        &descriptor_set_info, &texture->set);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_view;
    }
  }
  {
    int i;
    uint64_t image_size;
    uint32_t upload_offset;
    uint64_t upload_size;

    upload_offset = 0;
    image_size = width * height * 4;
    upload_size = width * height * 4 * 6;
    upload_data = owl_renderer_upload_allocate(renderer, upload_size,
        &upload_alloc);
    if (!upload_data) {
      code = OWL_ERROR_NO_UPLOAD_MEMORY;
      goto error_free_descriptor_sets;
    }

    for (i = 0; i < 6; ++i) {
      if (0 < i) {
        data = stbi_load(desc->files[i], &width, &height, &chans,
            STBI_rgb_alpha);
        if (!data) {
          code = OWL_ERROR_NOT_FOUND;
          goto error_free_upload_data;
        }
      }

      copies[i].bufferOffset = upload_offset;
      copies[i].bufferRowLength = 0;
      copies[i].bufferImageHeight = 0;
      copies[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copies[i].imageSubresource.mipLevel = 0;
      copies[i].imageSubresource.baseArrayLayer = i;
      copies[i].imageSubresource.layerCount = 1;
      copies[i].imageOffset.x = 0;
      copies[i].imageOffset.y = 0;
      copies[i].imageOffset.z = 0;
      copies[i].imageExtent.width = (uint32_t)width;
      copies[i].imageExtent.height = (uint32_t)height;
      copies[i].imageExtent.depth = 1;

      OWL_MEMCPY(upload_data + upload_offset, data, image_size);
      stbi_image_free(data);
      data = NULL;
    }
  }

  code = owl_renderer_begin_immediate_command_buffer(renderer);
  if (code)
    goto error_free_upload_data;

  owl_renderer_texture_cube_transition(texture, renderer,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage(renderer->immediate_command_buffer,
      upload_alloc.buffer, texture->image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies);

  owl_renderer_texture_cube_transition(texture, renderer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  code = owl_renderer_end_immediate_command_buffer(renderer);
  if (code)
    goto error_free_upload_data;

  {
    VkDescriptorImageInfo descriptors[2];
    VkWriteDescriptorSet writes[2];

    descriptors[0].sampler = renderer->linear_sampler;
    descriptors[0].imageView = NULL;
    descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptors[1].sampler = VK_NULL_HANDLE;
    descriptors[1].imageView = texture->image_view;
    descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = texture->set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &descriptors[0];
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = texture->set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &descriptors[1];
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, OWL_ARRAY_SIZE(writes), writes, 0,
        NULL);
  }

  owl_renderer_upload_free(renderer, upload_data);

  goto out;

error_free_upload_data:
  owl_renderer_upload_free(renderer, upload_data);

error_free_descriptor_sets:
  vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
      &texture->set);

error_destroy_image_view:
  vkDestroyImageView(renderer->device, texture->image_view, NULL);

error_free_memory:
  vkFreeMemory(renderer->device, texture->memory, NULL);

error_destroy_image:
  vkDestroyImage(renderer->device, texture->image, NULL);

out:
  if (data)
    stbi_image_free(data);

  return code;
}

OWL_PUBLIC void
owl_renderer_texture_cube_deinit(struct owl_renderer_texture_cube *texture,
    struct owl_renderer *renderer) {
  vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
      &texture->set);
  vkDestroyImageView(renderer->device, texture->image_view, NULL);
  vkFreeMemory(renderer->device, texture->memory, NULL);
  vkDestroyImage(renderer->device, texture->image, NULL);
}

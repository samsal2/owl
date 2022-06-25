#include "owl_texture.h"

#include "owl_internal.h"
#include "owl_renderer.h"
#include "stb_image.h"
#include "vulkan/vulkan_core.h"

#include <math.h>

#define OWL_TEXTURE_MAX_PATH_LENGTH 128

static VkFormat
owl_pixel_format_as_vk_format(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

static uint64_t
owl_pixel_format_size(enum owl_pixel_format format) {
  switch (format) {
  case OWL_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof(uint8_t);

  case OWL_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(uint8_t);
  }
}

static uint32_t
owl_texture_calculate_mipmaps(struct owl_texture *texture) {
  return (uint32_t)(floor(log2(OWL_MAX(texture->width, texture->height))) + 1);
}

static void
owl_texture_change_layout(struct owl_texture *texture,
                          struct owl_renderer *renderer,
                          VkImageLayout layout) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst_Stage = VK_PIPELINE_STAGE_NONE_KHR;
  VkCommandBuffer command_buffer = renderer->immediate_command_buffer;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = texture->layout;
  barrier.newLayout = layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = texture->image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = texture->mipmaps;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = texture->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_Stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == texture->layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_Stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == texture->layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_Stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier(command_buffer, src_stage, dst_Stage, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

  texture->layout = layout;
}

static void
owl_texture_generate_mipmaps(struct owl_texture *texture,
                             struct owl_renderer *renderer) {
  int32_t i;
  int32_t width;
  int32_t height;
  VkImageMemoryBarrier barrier;
  VkCommandBuffer command_buffer = renderer->immediate_command_buffer;

  if (1 == texture->mipmaps || 0 == texture->mipmaps)
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
  barrier.subresourceRange.layerCount = texture->layers;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (int32_t)(texture->mipmaps - 1); ++i) {
    VkImageBlit blit;

    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

    vkCmdBlitImage(command_buffer, texture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = texture->mipmaps - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

  texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

/* TODO(samuel): testing new cleanup style, refactor later */
OWL_PUBLIC owl_code
owl_texture_init(struct owl_renderer *renderer, struct owl_texture_desc *desc,
                 struct owl_texture *texture) {
  owl_code code;

  VkFormat vk_format;
  VkResult vk_result;

  uint8_t *upload_data = NULL;
  struct owl_renderer_upload_allocation upload_allocation;

  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->image = VK_NULL_HANDLE;
  texture->memory = VK_NULL_HANDLE;
  texture->image_view = VK_NULL_HANDLE;
  texture->descriptor_set = VK_NULL_HANDLE;

  /* first upload the image(s) data into the upload buffer */
  if (OWL_TEXTURE_SOURCE_DATA == desc->source) {
    uint64_t bitmap_size;
    uint64_t pixel_size;

    /* loading a cubemap from raw data not supported atm */
    if (OWL_TEXTURE_TYPE_CUBE == desc->type) {
      code = OWL_ERROR_INVALID_VALUE;
      goto error;
    }

    texture->layers = 1;

    /* set the texture dimensions */
    texture->width = desc->width;
    texture->height = desc->height;

    /* translate to the vk_format enum */
    vk_format = owl_pixel_format_as_vk_format(desc->format);

    /* calculate the size in bytes */
    pixel_size = owl_pixel_format_size(desc->format);
    bitmap_size = texture->width * texture->height * pixel_size;

    /* allocate staging memory */
    upload_data = owl_renderer_upload_allocate(renderer, bitmap_size,
                                               &upload_allocation);
    if (!upload_data) {
      code = OWL_ERROR_NO_MEMORY;
      goto error;
    }

    /* copy the data into the buffer */
    OWL_MEMCPY(upload_data, desc->pixels, bitmap_size);
  } else if (OWL_TEXTURE_SOURCE_FILE == desc->source) {
    /* when loading from disk, always use r8g8b8a8_srgb */
    vk_format = owl_pixel_format_as_vk_format(OWL_PIXEL_FORMAT_R8G8B8A8_SRGB);

    /* if it's just one 2D texture, simpling use stbi_load and load */
    if (OWL_TEXTURE_TYPE_2D == desc->type) {
      int width;
      int height;
      int channels;
      uint8_t *data;
      uint64_t bitmap_size;
      uint64_t pixel_size;

      texture->layers = 1;

      /* load image from disk */
      data = stbi_load(desc->path, &width, &height, &channels, STBI_rgb_alpha);
      if (!data) {
        code = OWL_ERROR_FATAL;
        goto error;
      }

      texture->width = width;
      texture->height = height;

      /* calculate the size in bytes */
      pixel_size = owl_pixel_format_size(OWL_PIXEL_FORMAT_R8G8B8A8_SRGB);
      bitmap_size = width * height * pixel_size;

      /* allocate in the upload buffer */
      upload_data = owl_renderer_upload_allocate(renderer, bitmap_size,
                                                 &upload_allocation);
      if (!upload_data) {
        stbi_image_free(data);
        code = OWL_ERROR_NO_MEMORY;
        goto error;
      }

      /* copy into the upload buffer */
      OWL_MEMCPY(upload_data, data, bitmap_size);

      /* free the data allocated by stb */
      stbi_image_free(data);

    } else if (OWL_TEXTURE_TYPE_CUBE == desc->type) {
      uint8_t *data;
      int32_t i;
      int width = 0;
      int height = 0;
      int channels = 0;
      uint64_t offset = 0;
      uint64_t bitmap_size = 0;
      uint64_t pixel_size;
      /* TODO(samuel): currently the images in the specified must have these
       * names and extensions, make this a non requirement */
      static char const *names[6] = {"left.jpg",   "right.jpg", "top.jpg",
                                     "bottom.jpg", "front.jpg", "back.jpg"};
      /* set the width and texture to 0 as a way to check if a width and height
       * has been loaded */
      texture->width = 0;
      texture->height = 0;
      texture->layers = 6;

      /* calculate the pixel size */
      pixel_size = owl_pixel_format_size(OWL_PIXEL_FORMAT_R8G8B8A8_SRGB);

      for (i = 0; i < (int32_t)OWL_ARRAY_SIZE(names); ++i) {
        char path[OWL_TEXTURE_MAX_PATH_LENGTH];

        /* get the exact path of the image */
        OWL_SNPRINTF(path, OWL_TEXTURE_MAX_PATH_LENGTH, "%s/%s", desc->path,
                     names[i]);

        /* load the image */
        data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
        if (!data) {
          OWL_ASSERT(0);
          code = OWL_ERROR_FATAL;
          goto error;
        }

        /* if texture->width and texture->height have been set (!= 0) */
        if (texture->width && texture->height) {
          /* the width and height loaded don't match, error out */
          if (!((int)texture->width == width &&
                (int)texture->height == height)) {
            OWL_ASSERT(0);
            stbi_image_free(data);
            code = OWL_ERROR_FATAL;
            goto error;
          }
        } else { /* if texture->width and texture->height have not been set  */
          /* set them to the current width and height */
          texture->width = width;
          texture->height = height;

          /* find the size in bytes */
          bitmap_size = width * height * pixel_size;

          /* allocate enought memory for 6 layers */
          upload_data = owl_renderer_upload_allocate(renderer, bitmap_size * 6,
                                                     &upload_allocation);
          if (!upload_data) { /* not enough memory, error out */
            stbi_image_free(data);
            code = OWL_ERROR_FATAL;
            goto error;
          }
        }

        /* copy the image into the upload buffer at the current offset */
        OWL_MEMCPY(upload_data + offset, data, bitmap_size);

        /* update the offset */
        offset += bitmap_size;

        /* free the data */
        stbi_image_free(data);
      }
    }
  } else {
    code = OWL_ERROR_FATAL;
    goto error;
  }

  texture->mipmaps = owl_texture_calculate_mipmaps(texture);

  /* create the vulkan resources */

  {
    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    if (OWL_TEXTURE_TYPE_CUBE == desc->type)
      image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    else
      image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = vk_format;
    image_create_info.extent.width = texture->width;
    image_create_info.extent.height = texture->height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = texture->mipmaps;
    image_create_info.arrayLayers = texture->layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = texture->layout;

    vk_result = vkCreateImage(renderer->device, &image_create_info, NULL,
                              &texture->image);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error;
    }
  }

  {
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetImageMemoryRequirements(renderer->device, texture->image,
                                 &memory_requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &texture->memory);
    if (vk_result) {
      code = OWL_ERROR_NO_MEMORY;
      goto error;
    }

    vk_result = vkBindImageMemory(renderer->device, texture->image,
                                  texture->memory, 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error;
    }
  }

  {
    VkImageViewCreateInfo image_view_create_info;

    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = texture->image;
    if (OWL_TEXTURE_TYPE_CUBE == desc->type)
      image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else
      image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = vk_format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = texture->layers;

    vk_result = vkCreateImageView(renderer->device, &image_view_create_info,
                                  NULL, &texture->image_view);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error;
    }
  }

  {
    VkDescriptorSetAllocateInfo descriptor_set_allocate_info;

    descriptor_set_allocate_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_allocate_info.pNext = NULL;
    descriptor_set_allocate_info.descriptorPool = renderer->descriptor_pool;
    descriptor_set_allocate_info.descriptorSetCount = 1;
    descriptor_set_allocate_info.pSetLayouts =
        &renderer->image_fragment_descriptor_set_layout;

    vk_result = vkAllocateDescriptorSets(renderer->device,
                                         &descriptor_set_allocate_info,
                                         &texture->descriptor_set);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error;
    }
  }

  code = owl_renderer_begin_immediate_command_buffer(renderer);
  if (code)
    goto error;

  owl_texture_change_layout(texture, renderer,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  if (OWL_TEXTURE_TYPE_2D == desc->type) {
    VkBufferImageCopy copy;

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
    copy.imageExtent.width = texture->width;
    copy.imageExtent.height = texture->height;
    copy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(renderer->immediate_command_buffer,
                           upload_allocation.buffer, texture->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  } else if (OWL_TEXTURE_TYPE_CUBE == desc->type) {
    int32_t i;
    uint64_t offset = 0;
    uint64_t pixel_size;
    uint64_t size;
    VkBufferImageCopy copies[6];

    pixel_size = owl_pixel_format_size(OWL_PIXEL_FORMAT_R8G8B8A8_SRGB);
    size = texture->width * texture->height * pixel_size;

    for (i = 0; i < (int32_t)OWL_ARRAY_SIZE(copies); ++i) {
      copies[i].bufferOffset = offset;
      copies[i].bufferRowLength = 0;
      copies[i].bufferImageHeight = 0;
      copies[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      copies[i].imageSubresource.mipLevel = 0;
      copies[i].imageSubresource.baseArrayLayer = i;
      copies[i].imageSubresource.layerCount = 1;
      copies[i].imageOffset.x = 0;
      copies[i].imageOffset.y = 0;
      copies[i].imageOffset.z = 0;
      copies[i].imageExtent.width = (uint32_t)texture->width;
      copies[i].imageExtent.height = (uint32_t)texture->height;
      copies[i].imageExtent.depth = 1;

      offset += size;
    }

    vkCmdCopyBufferToImage(renderer->immediate_command_buffer,
                           upload_allocation.buffer, texture->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           OWL_ARRAY_SIZE(copies), copies);
  }

  /* NOTE(samuel): this transfers the layout to shared read optimal */
  owl_texture_generate_mipmaps(texture, renderer);

  code = owl_renderer_end_immediate_command_buffer(renderer);
  if (code)
    goto error;

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

    vkUpdateDescriptorSets(renderer->device, OWL_ARRAY_SIZE(writes), writes, 0,
                           NULL);
  }

  owl_renderer_upload_free(renderer, upload_data);

  return OWL_OK;

error:
  if (texture->descriptor_set) {
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &texture->descriptor_set);
    texture->descriptor_set = VK_NULL_HANDLE;
  }

  if (texture->image_view) {
    vkDestroyImageView(renderer->device, texture->image_view, NULL);
    texture->image_view = VK_NULL_HANDLE;
  }

  if (texture->memory) {
    vkFreeMemory(renderer->device, texture->memory, NULL);
    texture->memory = VK_NULL_HANDLE;
  }

  if (texture->image) {
    vkDestroyImage(renderer->device, texture->image, NULL);
    texture->image = VK_NULL_HANDLE;
  }

  if (upload_data)
    owl_renderer_upload_free(renderer, upload_data);

  return code;
}

OWL_PUBLIC void
owl_texture_deinit(struct owl_renderer *renderer,
                   struct owl_texture *texture) {
  vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                       &texture->descriptor_set);
  vkDestroyImageView(renderer->device, texture->image_view, NULL);
  vkFreeMemory(renderer->device, texture->memory, NULL);
  vkDestroyImage(renderer->device, texture->image, NULL);
}

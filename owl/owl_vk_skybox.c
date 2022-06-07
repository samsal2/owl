#include "owl_vk_skybox.h"

#include "owl_internal.h"
#include "owl_vk_misc.h"
#include "owl_vk_renderer.h"
#include "owl_vk_texture.h"
#include "owl_vk_upload.h"

#include "stb_image.h"

#define OWL_VK_SKYBOX_MAX_PATH_LENGTH 256

owl_private void
owl_vk_skybox_transition(struct owl_vk_renderer *vk, VkImageLayout src_layout,
                         VkImageLayout dst_layout) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_NONE_KHR;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = src_layout;
  barrier.newLayout = dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = vk->skybox_image;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 6;

  if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == src_layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    owl_assert(0 && "Invalid arguments");
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    owl_assert(0 && "Invalid arguments");
  }

  vkCmdPipelineBarrier(vk->im_command_buffer, src_stage, dst_stage, 0, 0, NULL,
                       0, NULL, 1, &barrier);
}

owl_public owl_code
owl_vk_skybox_load(struct owl_vk_renderer *vk, char const *path) {
  int i;
  int width;
  int height;
  int chans;
  uint64_t image_size;
  uint8_t *data = NULL;

  int ret;
  char file[OWL_VK_SKYBOX_MAX_PATH_LENGTH];

  VkImageCreateInfo image_info;
  VkMemoryRequirements mem_req;
  VkMemoryAllocateInfo mem_info;
  VkImageViewCreateInfo image_view_info;
  VkDescriptorSetAllocateInfo set_info;
  VkDescriptorImageInfo descriptors[2];
  VkWriteDescriptorSet writes[2];

  uint32_t upload_offset;
  uint8_t *upload_data;
  uint64_t upload_size;
  struct owl_vk_upload_allocation upload_alloc;

  VkBufferImageCopy copies[6];

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  owl_local_persist char const *names[6] = {"left.jpg",  "right.jpg",
                                            "top.jpg",   "bottom.jpg",
                                            "front.jpg", "back.jpg"};

  if (vk->skybox_loaded)
    owl_vk_skybox_unload(vk);

  ret = snprintf(file, owl_array_size(file), "%s/%s", path, names[0]);
  if (0 > ret) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  data = stbi_load(file, &width, &height, &chans, STBI_rgb_alpha);
  if (!data) {
    code = OWL_ERROR_NOT_FOUND;
    goto out;
  }

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

  vk_result = vkCreateImage(vk->device, &image_info, NULL, &vk->skybox_image);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetImageMemoryRequirements(vk->device, vk->skybox_image, &mem_req);

  mem_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_info.pNext = NULL;
  mem_info.allocationSize = mem_req.size;
  mem_info.memoryTypeIndex = owl_vk_find_memory_type(
      vk, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result =
      vkAllocateMemory(vk->device, &mem_info, NULL, &vk->skybox_memory);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_image;
  }

  vk_result =
      vkBindImageMemory(vk->device, vk->skybox_image, vk->skybox_memory, 0);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = vk->skybox_image;
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

  vk_result = vkCreateImageView(vk->device, &image_view_info, NULL,
                                &vk->skybox_image_view);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_info.pNext = NULL;
  set_info.descriptorPool = vk->descriptor_pool;
  set_info.descriptorSetCount = 1;
  set_info.pSetLayouts = &vk->image_fragment_set_layout;

  vk_result = vkAllocateDescriptorSets(vk->device, &set_info, &vk->skybox_set);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_image_view;
  }

  upload_offset = 0;
  image_size = width * height * 4;
  upload_size = width * height * 4 * owl_array_size(names);
  upload_data = owl_vk_upload_alloc(vk, upload_size, &upload_alloc);
  if (!upload_data) {
    code = OWL_ERROR_NO_UPLOAD_MEMORY;
    goto error_free_set;
  }

  for (i = 0; i < (int)owl_array_size(names); ++i) {
    if (0 < i) {
      ret = snprintf(file, OWL_VK_SKYBOX_MAX_PATH_LENGTH, "%s/%s", path,
                     names[i]);
      data = stbi_load(file, &width, &height, &chans, STBI_rgb_alpha);
      if (!data) {
        code = OWL_ERROR_NOT_FOUND;
        goto error_free_upload;
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

    owl_memcpy(upload_data + upload_offset, data, image_size);
    stbi_image_free(data);
    data = NULL;
  }

  code = owl_vk_begin_im_command_buffer(vk);
  if (code)
    goto error_free_upload;

  owl_vk_skybox_transition(vk, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkCmdCopyBufferToImage(vk->im_command_buffer, upload_alloc.buffer,
                         vk->skybox_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copies);

  owl_vk_skybox_transition(vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  code = owl_vk_end_im_command_buffer(vk);
  if (code)
    goto error_free_upload;

  descriptors[0].sampler = vk->linear_sampler;
  descriptors[0].imageView = NULL;
  descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  descriptors[1].sampler = VK_NULL_HANDLE;
  descriptors[1].imageView = vk->skybox_image_view;
  descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].pNext = NULL;
  writes[0].dstSet = vk->skybox_set;
  writes[0].dstBinding = 0;
  writes[0].dstArrayElement = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  writes[0].pImageInfo = &descriptors[0];
  writes[0].pBufferInfo = NULL;
  writes[0].pTexelBufferView = NULL;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].pNext = NULL;
  writes[1].dstSet = vk->skybox_set;
  writes[1].dstBinding = 1;
  writes[1].dstArrayElement = 0;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  writes[1].pImageInfo = &descriptors[1];
  writes[1].pBufferInfo = NULL;
  writes[1].pTexelBufferView = NULL;

  vkUpdateDescriptorSets(vk->device, owl_array_size(writes), writes, 0, NULL);

  owl_vk_upload_free(vk, upload_data);

  vk->skybox_loaded = 1;

  goto out;

error_free_upload:
  owl_vk_upload_free(vk, upload_data);

error_free_set:
  vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1, &vk->skybox_set);

error_destroy_image_view:
  vkDestroyImageView(vk->device, vk->skybox_image_view, NULL);

error_free_memory:
  vkFreeMemory(vk->device, vk->skybox_memory, NULL);

error_destroy_image:
  vkDestroyImage(vk->device, vk->skybox_image, NULL);

out:
  if (data)
    stbi_image_free(data);

  return code;
}

owl_public void
owl_vk_skybox_unload(struct owl_vk_renderer *vk) {
  vk->skybox_loaded = 0;
  vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1, &vk->skybox_set);
  vkDestroyImageView(vk->device, vk->skybox_image_view, NULL);
  vkFreeMemory(vk->device, vk->skybox_memory, NULL);
  vkDestroyImage(vk->device, vk->skybox_image, NULL);
}

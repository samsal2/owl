#include "skybox.h"

#include "internal.h"
#include "renderer.h"
#include "texture.h"

#define OWL_MAX_ANISOTROPY 16
#define OWL_SKYBOX_FACE_COUNT 6
#define OWL_SKYBOX_NO_DIM (owl_u32) - 1

struct owl_skybox_loading_info {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  owl_byte *right;
  owl_byte *left;
  owl_byte *top;
  owl_byte *bottom;
  owl_byte *back;
  owl_byte *front;
  enum owl_pixel_format format;
};

OWL_INTERNAL void
owl_skybox_copy_to_image_(VkCommandBuffer command, owl_u32 base_width,
                          owl_u32 base_height, owl_u32 face, owl_u32 level,
                          struct owl_dynamic_heap_reference const *dhr,
                          struct owl_skybox const *box) {

  VkBufferImageCopy copy;

  copy.bufferOffset = dhr->offset;
  copy.bufferRowLength = 0;
  copy.bufferImageHeight = 0;
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.mipLevel = level;
  copy.imageSubresource.baseArrayLayer = face;
  copy.imageSubresource.layerCount = 1;
  copy.imageOffset.x = 0;
  copy.imageOffset.y = 0;
  copy.imageOffset.z = 0;
  copy.imageExtent.width = base_width >> level;
  copy.imageExtent.height = base_height >> level;
  copy.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(command, dhr->buffer, box->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

OWL_INTERNAL enum owl_code
owl_skybox_init_loading_info_(struct owl_skybox_init_info const *sii,
                              struct owl_skybox_loading_info *sli) {

  struct owl_texture_init_info tii;
  enum owl_code code = OWL_SUCCESS;

  sli->right = owl_texture_data_from_file(sii->right, &tii);

  if (!sii->right)
    goto end_err;

  sli->width = (owl_u32)tii.width;
  sli->height = (owl_u32)tii.height;
  sli->format = (owl_u32)tii.format;

  sli->left = owl_texture_data_from_file(sii->left, &tii);

  if (!sli->left)
    goto end_err_free_right;

  if (sli->width != (owl_u32)tii.width)
    goto end_err_free_left;

  if (sli->height != (owl_u32)tii.height)
    goto end_err_free_left;

  if (sli->format != (owl_u32)tii.format)
    goto end_err_free_left;

  sli->top = owl_texture_data_from_file(sii->top, &tii);

  if (!sli->top)
    goto end_err_free_left;

  if (sli->width != (owl_u32)tii.width)
    goto end_err_free_top;

  if (sli->height != (owl_u32)tii.height)
    goto end_err_free_top;

  if (sli->format != (owl_u32)tii.format)
    goto end_err_free_top;

  sli->bottom = owl_texture_data_from_file(sii->bottom, &tii);

  if (!sli->bottom)
    goto end_err_free_top;

  if (sli->width != (owl_u32)tii.width)
    goto end_err_free_bottom;

  if (sli->height != (owl_u32)tii.height)
    goto end_err_free_bottom;

  if (sli->format != (owl_u32)tii.format)
    goto end_err_free_bottom;

  sli->back = owl_texture_data_from_file(sii->back, &tii);

  if (!sli->back)
    goto end_err_free_bottom;

  if (sli->width != (owl_u32)tii.width)
    goto end_err_free_back;

  if (sli->height != (owl_u32)tii.height)
    goto end_err_free_back;

  if (sli->format != (owl_u32)tii.format)
    goto end_err_free_back;

  sli->front = owl_texture_data_from_file(sii->front, &tii);

  if (!sli->front)
    goto end_err_free_back;

  if (sli->width != (owl_u32)tii.width)
    goto end_err_free_front;

  if (sli->height != (owl_u32)tii.height)
    goto end_err_free_front;

  if (sli->format != (owl_u32)tii.format)
    goto end_err_free_front;

  sli->mips = owl_calc_mips(sli->width, sli->height);

  goto end;

end_err_free_front:
  owl_texture_free_data_from_file(sli->front);

end_err_free_back:
  owl_texture_free_data_from_file(sli->back);

end_err_free_bottom:
  owl_texture_free_data_from_file(sli->bottom);

end_err_free_top:
  owl_texture_free_data_from_file(sli->top);

end_err_free_left:
  owl_texture_free_data_from_file(sli->left);

end_err_free_right:
  owl_texture_free_data_from_file(sli->right);

end_err:
  code = OWL_ERROR_UNKNOWN;

end:
  return code;
}

OWL_INTERNAL void
owl_skybox_deinit_loading_info_(struct owl_skybox_loading_info *info) {
  owl_texture_free_data_from_file(info->right);
  owl_texture_free_data_from_file(info->left);
  owl_texture_free_data_from_file(info->top);
  owl_texture_free_data_from_file(info->bottom);
  owl_texture_free_data_from_file(info->back);
  owl_texture_free_data_from_file(info->front);
}

OWL_INTERNAL enum owl_code owl_skybox_copy_loading_info_to_image_(
    struct owl_renderer *r, struct owl_skybox_loading_info const *sli,
    struct owl_skybox const *box) {
  owl_u32 i;
  VkDeviceSize size;
  struct owl_single_use_command_buffer sucb;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_dynamic_heap_is_offset_clear(r));

  {
    struct owl_texture_init_info tii;

    tii.width = (int)sli->width;
    tii.height = (int)sli->height;
    tii.format = sli->format;

    size = owl_texture_init_info_required_size_(&tii);
  }

  owl_renderer_init_single_use_command_buffer(r, &sucb);

  {
    struct owl_vk_image_transition_info iti;

    iti.mips = sli->mips;
    iti.layers = OWL_SKYBOX_FACE_COUNT;
    iti.from = VK_IMAGE_LAYOUT_UNDEFINED;
    iti.to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    iti.image = box->image;
    iti.command_buffer = sucb.command_buffer;

    owl_vk_image_transition(&iti);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->right, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 0,
                                i, &dhr, box);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->left, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 1,
                                i, &dhr, box);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->bottom, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 2,
                                i, &dhr, box);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->top, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 3,
                                i, &dhr, box);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->back, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 4,
                                i, &dhr, box);
  }

  {
    struct owl_dynamic_heap_reference dhr;
    owl_byte *staging = owl_renderer_dynamic_heap_alloc(r, size, &dhr);

    OWL_MEMCPY(staging, sli->front, size);

    for (i = 0; i < sli->mips; ++i)
      owl_skybox_copy_to_image_(sucb.command_buffer, sli->width, sli->height, 5,
                                i, &dhr, box);
  }

  {
    struct owl_vk_image_transition_info iti;

    iti.mips = sli->mips;
    iti.layers = OWL_SKYBOX_FACE_COUNT;
    iti.from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    iti.to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    iti.image = box->image;
    iti.command_buffer = sucb.command_buffer;

    owl_vk_image_transition(&iti);
  }

  owl_renderer_deinit_single_use_command_buffer(r, &sucb);
  owl_renderer_dynamic_heap_clear_offset(r);

  return code;
}

enum owl_code owl_skybox_init(struct owl_renderer *r,
                              struct owl_skybox_init_info const *sii,
                              struct owl_skybox *box) {
  struct owl_skybox_loading_info sli;
  enum owl_code code = OWL_SUCCESS;

  code = owl_skybox_init_loading_info_(sii, &sli);

  if (OWL_SUCCESS != code)
    goto end;

  {
    VkImageCreateInfo image;

    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_as_vk_format_(sli.format);
    image.extent.width = (owl_u32)sli.width;
    image.extent.height = (owl_u32)sli.height;
    image.extent.depth = 1;
    image.mipLevels = sli.mips;
    image.arrayLayers = OWL_SKYBOX_FACE_COUNT;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(r->device, &image, NULL, &box->image));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetImageMemoryRequirements(r->device, box->image, &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_VK_CHECK(vkAllocateMemory(r->device, &memory, NULL, &box->memory));
    OWL_VK_CHECK(vkBindImageMemory(r->device, box->image, box->memory, 0));
  }

  owl_skybox_copy_loading_info_to_image_(r, &sli, box);

  {
    VkSamplerCreateInfo sampler;
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
    sampler.maxLod = sli.mips; /* VK_LOD_CLAMP_NONE */
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;

    OWL_VK_CHECK(vkCreateSampler(r->device, &sampler, NULL, &box->sampler));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = box->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format = owl_as_vk_format_(sli.format);
    view.components.r = VK_COMPONENT_SWIZZLE_R;
    view.components.g = VK_COMPONENT_SWIZZLE_G;
    view.components.b = VK_COMPONENT_SWIZZLE_B;
    view.components.a = VK_COMPONENT_SWIZZLE_A;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = sli.mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = OWL_SKYBOX_FACE_COUNT;

    OWL_VK_CHECK(vkCreateImageView(r->device, &view, NULL, &box->view));
  }

  {
    VkDescriptorSetLayout layout;
    VkDescriptorSetAllocateInfo set;

    layout = r->texture_set_layout;

    set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set.pNext = NULL;
    set.descriptorPool = r->common_set_pool;
    set.descriptorSetCount = 1;
    set.pSetLayouts = &layout;

    OWL_VK_CHECK(vkAllocateDescriptorSets(r->device, &set, &box->set));
  }

  {
    VkDescriptorImageInfo images[2];
    VkWriteDescriptorSet writes[2];

    images[0].sampler = box->sampler;
    images[0].imageView = VK_NULL_HANDLE;
    images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    images[1].sampler = VK_NULL_HANDLE;
    images[1].imageView = box->view;
    images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = box->set;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &images[0];
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = box->set;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &images[1];
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0, NULL);
  }

  owl_skybox_deinit_loading_info_(&sli);

end:
  return code;
}

void owl_skybox_deinit(struct owl_renderer *r, struct owl_skybox *box) {
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeDescriptorSets(r->device, r->common_set_pool, 1, &box->set);
  vkDestroySampler(r->device, box->sampler, NULL);
  vkDestroyImageView(r->device, box->view, NULL);
  vkFreeMemory(r->device, box->memory, NULL);
  vkDestroyImage(r->device, box->image, NULL);
}

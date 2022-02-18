#include "skybox.h"

#include "internal.h"
#include "renderer.h"
#include "texture.h"

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
owl_skybox_copy_to_image_(VkCommandBuffer cmd, owl_u32 base_width,
                          owl_u32 base_height, owl_u32 face, owl_u32 level,
                          struct owl_dynamic_buffer_reference const *ref,
                          struct owl_skybox const *box) {

  VkBufferImageCopy copy;

  copy.bufferOffset = ref->offset;
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

  vkCmdCopyBufferToImage(cmd, ref->buffer, box->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

OWL_INTERNAL enum owl_code
owl_skybox_init_loading_info_(struct owl_skybox_info const *info,
                              struct owl_skybox_loading_info *loading_info) {

  struct owl_texture_info texture_info;
  enum owl_code code = OWL_SUCCESS;

  loading_info->right = owl_texture_data_from_file(info->right, &texture_info);

  if (!info->right)
    goto end_err;

  loading_info->width = (owl_u32)texture_info.width;
  loading_info->height = (owl_u32)texture_info.height;
  loading_info->format = (owl_u32)texture_info.format;

  loading_info->left = owl_texture_data_from_file(info->left, &texture_info);

  if (!loading_info->left)
    goto end_err_free_right;

  if (loading_info->width != (owl_u32)texture_info.width)
    goto end_err_free_left;

  if (loading_info->height != (owl_u32)texture_info.height)
    goto end_err_free_left;

  if (loading_info->format != (owl_u32)texture_info.format)
    goto end_err_free_left;

  loading_info->top = owl_texture_data_from_file(info->top, &texture_info);

  if (!loading_info->top)
    goto end_err_free_left;

  if (loading_info->width != (owl_u32)texture_info.width)
    goto end_err_free_top;

  if (loading_info->height != (owl_u32)texture_info.height)
    goto end_err_free_top;

  if (loading_info->format != (owl_u32)texture_info.format)
    goto end_err_free_top;

  loading_info->bottom =
      owl_texture_data_from_file(info->bottom, &texture_info);

  if (!loading_info->bottom)
    goto end_err_free_top;

  if (loading_info->width != (owl_u32)texture_info.width)
    goto end_err_free_bottom;

  if (loading_info->height != (owl_u32)texture_info.height)
    goto end_err_free_bottom;

  if (loading_info->format != (owl_u32)texture_info.format)
    goto end_err_free_bottom;

  loading_info->back = owl_texture_data_from_file(info->back, &texture_info);

  if (!loading_info->back)
    goto end_err_free_bottom;

  if (loading_info->width != (owl_u32)texture_info.width)
    goto end_err_free_back;

  if (loading_info->height != (owl_u32)texture_info.height)
    goto end_err_free_back;

  if (loading_info->format != (owl_u32)texture_info.format)
    goto end_err_free_back;

  loading_info->front = owl_texture_data_from_file(info->front, &texture_info);

  if (!loading_info->front)
    goto end_err_free_back;

  if (loading_info->width != (owl_u32)texture_info.width)
    goto end_err_free_front;

  if (loading_info->height != (owl_u32)texture_info.height)
    goto end_err_free_front;

  if (loading_info->format != (owl_u32)texture_info.format)
    goto end_err_free_front;

  loading_info->mips = owl_calc_mips(loading_info->width, loading_info->height);

  goto end;

end_err_free_front:
  owl_texture_free_data_from_file(loading_info->front);

end_err_free_back:
  owl_texture_free_data_from_file(loading_info->back);

end_err_free_bottom:
  owl_texture_free_data_from_file(loading_info->bottom);

end_err_free_top:
  owl_texture_free_data_from_file(loading_info->top);

end_err_free_left:
  owl_texture_free_data_from_file(loading_info->left);

end_err_free_right:
  owl_texture_free_data_from_file(loading_info->right);

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
    struct owl_renderer *r, struct owl_skybox_loading_info const *info,
    struct owl_skybox const *box) {
  owl_u32 i;
  VkCommandBuffer cmd;
  VkDeviceSize size;
  struct owl_vk_image_transition_info transition;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(owl_renderer_is_dynamic_buffer_clear(r));

  {
    struct owl_texture_info texture_info;
    texture_info.width = (int)info->width;
    texture_info.height = (int)info->height;
    texture_info.format = info->format;
    size = owl_info_required_size_(&texture_info);
  }

  transition.mips = info->mips;
  transition.layers = OWL_SKYBOX_FACE_COUNT;
  transition.from = VK_IMAGE_LAYOUT_UNDEFINED;
  transition.to = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  transition.image = box->image;

  owl_renderer_alloc_single_use_cmd_buffer(r, &cmd);
  owl_vk_image_transition(cmd, &transition);

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->right, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 0, i, &ref,
                                box);
  }

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->left, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 1, i, &ref,
                                box);
  }

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->bottom, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 2, i, &ref,
                                box);
  }

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->top, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 3, i, &ref,
                                box);
  }

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->back, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 4, i, &ref,
                                box);
  }

  {
    struct owl_dynamic_buffer_reference ref;
    owl_byte *staging = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(staging, info->front, size);

    for (i = 0; i < info->mips; ++i)
      owl_skybox_copy_to_image_(cmd, info->width, info->height, 5, i, &ref,
                                box);
  }

  transition.from = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  transition.to = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  owl_vk_image_transition(cmd, &transition);
  owl_renderer_free_single_use_cmd_buffer(r, cmd);

  owl_renderer_clear_dynamic_offset(r);

  return code;
}

enum owl_code owl_skybox_init(struct owl_renderer *r,
                              struct owl_skybox_info const *info,
                              struct owl_skybox *box) {
  struct owl_skybox_loading_info loading_info;
  enum owl_code code = OWL_SUCCESS;

  code = owl_skybox_init_loading_info_(info, &loading_info);

  if (OWL_SUCCESS != code)
    goto end;

  {
#define OWL_IMAGE_USAGE                                                        \
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |          \
      VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo image;

    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = owl_as_vk_format_(loading_info.format);
    image.extent.width = (owl_u32)loading_info.width;
    image.extent.height = (owl_u32)loading_info.height;
    image.extent.depth = 1;
    image.mipLevels = loading_info.mips;
    image.arrayLayers = OWL_SKYBOX_FACE_COUNT;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = OWL_IMAGE_USAGE;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_CHECK(vkCreateImage(r->device, &image, NULL, &box->image));

#undef OWL_IMAGE_USAGE
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetImageMemoryRequirements(r->device, box->image, &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_CHECK(vkAllocateMemory(r->device, &memory, NULL, &box->memory));
    OWL_CHECK(vkBindImageMemory(r->device, box->image, box->memory, 0));
  }

  owl_skybox_copy_loading_info_to_image_(r, &loading_info, box);

  {
#define OWL_MAX_ANISOTROPY 16

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
    sampler.maxLod = loading_info.mips; /* VK_LOD_CLAMP_NONE */
    sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler.unnormalizedCoordinates = VK_FALSE;

    OWL_CHECK(vkCreateSampler(r->device, &sampler, NULL, &box->sampler));

#undef OWL_MAX_ANISOTROPY
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = box->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format = owl_as_vk_format_(loading_info.format);
    view.components.r = VK_COMPONENT_SWIZZLE_R;
    view.components.g = VK_COMPONENT_SWIZZLE_G;
    view.components.b = VK_COMPONENT_SWIZZLE_B;
    view.components.a = VK_COMPONENT_SWIZZLE_A;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = loading_info.mips;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = OWL_SKYBOX_FACE_COUNT;

    OWL_CHECK(vkCreateImageView(r->device, &view, NULL, &box->view));
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

    OWL_CHECK(vkAllocateDescriptorSets(r->device, &set, &box->set));
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

  owl_skybox_deinit_loading_info_(&loading_info);

end:
  return code;
}

void owl_skybox_deinit(struct owl_renderer *r, struct owl_skybox *box) {
  OWL_CHECK(vkDeviceWaitIdle(r->device));

  vkFreeDescriptorSets(r->device, r->common_set_pool, 1, &box->set);
  vkDestroySampler(r->device, box->sampler, NULL);
  vkDestroyImageView(r->device, box->view, NULL);
  vkFreeMemory(r->device, box->memory, NULL);
  vkDestroyImage(r->device, box->image, NULL);
}
enum owl_code owl_skybox_create(struct owl_renderer *r,
                                struct owl_skybox_info const *info,
                                struct owl_skybox **box) {
  enum owl_code code = OWL_SUCCESS;

  if (!(*box = OWL_MALLOC(sizeof(**box)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  code = owl_skybox_init(r, info, *box);

  if (OWL_SUCCESS != code)
    OWL_FREE(*box);

end:
  return code;
}

void owl_skybox_destroy(struct owl_renderer *r, struct owl_skybox *box) {
  owl_skybox_deinit(r, box);
  OWL_FREE(box);
}

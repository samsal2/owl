#include "owl_vk_frame_heap.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_vk_context.h"
#include "owl_vk_pipeline_manager.h"
#include "owl_vk_types.h"

owl_public enum owl_code
owl_vk_frame_heap_init (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx, owl_u64 sz)
{
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo memory_info;
  VkDescriptorSetAllocateInfo set_info;
  VkDescriptorBufferInfo descriptor;
  VkWriteDescriptorSet write;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (sz);

  heap->offset = 0;
  heap->size = sz;

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = sz;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = NULL;

  vk_result =
      vkCreateBuffer (ctx->vk_device, &buffer_info, NULL, &heap->vk_buffer);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkGetBufferMemoryRequirements (ctx->vk_device, heap->vk_buffer, &req);

  heap->alignment = req.alignment;

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = req.size;
  memory_info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_CPU_ONLY);

  vk_result =
      vkAllocateMemory (ctx->vk_device, &memory_info, NULL, &heap->vk_memory);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_buffer_deinit;
  }

  vk_result =
      vkBindBufferMemory (ctx->vk_device, heap->vk_buffer, heap->vk_memory, 0);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  vk_result =
      vkMapMemory (ctx->vk_device, heap->vk_memory, 0, sz, 0, &heap->data);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_info.pNext = NULL;
  set_info.descriptorPool = ctx->vk_set_pool;
  set_info.descriptorSetCount = 1;
  set_info.pSetLayouts = &ctx->vk_vert_ubo_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &set_info,
                                        &heap->vk_pvm_ubo_set);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  set_info.pSetLayouts = &ctx->vk_both_ubo_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &set_info,
                                        &heap->vk_model_ubo1_set);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_pvm_ubo_set_deinit;
  }

  set_info.pSetLayouts = &ctx->vk_frag_ubo_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &set_info,
                                        &heap->vk_model_ubo2_set);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_model_ubo1_set_deinit;
  }

  descriptor.buffer = heap->vk_buffer;
  descriptor.offset = 0;
  descriptor.range = sizeof (struct owl_pvm_ubo);

  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = NULL;
  write.dstSet = heap->vk_pvm_ubo_set;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  write.pImageInfo = NULL;
  write.pBufferInfo = &descriptor;
  write.pTexelBufferView = NULL;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  descriptor.range = sizeof (struct owl_model_ubo1);
  write.dstSet = heap->vk_model_ubo1_set;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  descriptor.range = sizeof (struct owl_model_ubo2);
  write.dstSet = heap->vk_model_ubo2_set;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  goto out;

out_error_model_ubo1_set_deinit:
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo1_set);

out_error_pvm_ubo_set_deinit:
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_pvm_ubo_set);

out_error_memory_deinit:
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);

out_error_buffer_deinit:
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);

out:
  return code;
}

owl_public void
owl_vk_frame_heap_deinit (struct owl_vk_frame_heap *heap,
                          struct owl_vk_context const *ctx)
{
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo2_set);
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo1_set);
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_pvm_ubo_set);
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);
}

owl_public void
owl_vk_frame_heap_unmap (struct owl_vk_frame_heap *heap,
                         struct owl_vk_context const *ctx)
{
  vkUnmapMemory (ctx->vk_device, heap->vk_memory);
  heap->data = NULL;
}

owl_public owl_b32
owl_vk_frame_heap_has_enough_space (struct owl_vk_frame_heap *heap, owl_u64 sz)
{
  return (sz + heap->offset) <= heap->size;
}

owl_public void *
owl_vk_frame_heap_allocate (struct owl_vk_frame_heap *heap,
                            struct owl_vk_context const *ctx, owl_u64 sz,
                            struct owl_vk_frame_allocation *allocation)
{
  owl_byte *data = NULL;

  owl_unused (ctx);

  /* Not enough space */
  if (!owl_vk_frame_heap_has_enough_space (heap, sz)) {
    allocation->offset32 = (owl_u32)heap->offset;
    allocation->offset = heap->offset;
    allocation->vk_buffer = VK_NULL_HANDLE;
    allocation->vk_pvm_ubo_set = VK_NULL_HANDLE;
    allocation->vk_model_ubo1_set = VK_NULL_HANDLE;
    allocation->vk_model_ubo2_set = VK_NULL_HANDLE;
  } else {
    data = &((owl_byte *)(heap->data))[heap->offset];

    allocation->offset32 = (owl_u32)heap->offset;
    allocation->offset = heap->offset;
    allocation->vk_buffer = heap->vk_buffer;
    allocation->vk_pvm_ubo_set = heap->vk_pvm_ubo_set;
    allocation->vk_model_ubo1_set = heap->vk_model_ubo1_set;
    allocation->vk_model_ubo2_set = heap->vk_model_ubo2_set;

    heap->offset = owl_alignu2 (heap->offset + sz, heap->alignment);
  }

  return data;
}

owl_public owl_u64
owl_vk_frame_heap_offset (struct owl_vk_frame_heap const *heap)
{
  return heap->offset;
}

owl_public void
owl_vk_frame_heap_free (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx)
{
  owl_unused (ctx);

  heap->offset = 0;
}

owl_public void
owl_vk_frame_heap_reset (struct owl_vk_frame_heap *heap)
{
  heap->offset = 0;
}

owl_public void
owl_vk_frame_heap_unsafe_copy (struct owl_vk_frame_heap *dst,
                               struct owl_vk_frame_heap const *src)
{
  dst->data = src->data;
  dst->offset = src->offset;
  dst->size = src->size;
  dst->alignment = src->alignment;
  dst->vk_memory = src->vk_memory;
  dst->vk_buffer = src->vk_buffer;
  dst->vk_pvm_ubo_set = src->vk_pvm_ubo_set;
  dst->vk_model_ubo1_set = src->vk_model_ubo1_set;
  dst->vk_model_ubo2_set = src->vk_model_ubo2_set;
}

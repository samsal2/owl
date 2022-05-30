#include "owl_vk_frame_heap.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_vk_context.h"
#include "owl_vk_pipeline_manager.h"
#include "owl_vk_types.h"

owl_public enum owl_code
owl_vk_frame_heap_buffer_init (struct owl_vk_frame_heap *heap,
                               struct owl_vk_context const *ctx,
                               owl_u64 size) {
  VkBufferCreateInfo info;

  VkResult vk_result = VK_SUCCESS;

  heap->size = size;

  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.size = size;
  info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.queueFamilyIndexCount = 0;
  info.pQueueFamilyIndices = NULL;

  vk_result = vkCreateBuffer (ctx->vk_device, &info, NULL, &heap->vk_buffer);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_public void
owl_vk_frame_heap_buffer_deinit (struct owl_vk_frame_heap *heap,
                                 struct owl_vk_context const *ctx) {
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);
}

owl_private enum owl_code
owl_vk_frame_heap_memory_init (struct owl_vk_frame_heap *heap,
                               struct owl_vk_context const *ctx) {
  VkMemoryRequirements req;
  VkMemoryAllocateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkGetBufferMemoryRequirements (ctx->vk_device, heap->vk_buffer, &req);

  heap->alignment = req.alignment;

  info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  info.pNext = NULL;
  info.allocationSize = req.size;
  info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_CPU_ONLY);

  vk_result = vkAllocateMemory (ctx->vk_device, &info, NULL, &heap->vk_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vk_result =
      vkBindBufferMemory (ctx->vk_device, heap->vk_buffer, heap->vk_memory, 0);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  vk_result = vkMapMemory (ctx->vk_device, heap->vk_memory, 0, heap->size, 0,
                           &heap->data);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  goto out;

error_memory_deinit:
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);

out:
  return code;
}

owl_private void
owl_vk_frame_heap_memory_deinit (struct owl_vk_frame_heap *heap,
                                 struct owl_vk_context const *ctx) {
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);
}

owl_public enum owl_code
owl_vk_frame_heap_sets_init (struct owl_vk_frame_heap *heap,
                             struct owl_vk_context const *ctx) {
  VkDescriptorSetAllocateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.pNext = NULL;
  info.descriptorPool = ctx->vk_set_pool;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &ctx->vk_vert_ubo_set_layout;

  vk_result =
      vkAllocateDescriptorSets (ctx->vk_device, &info, &heap->vk_pvm_ubo);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  info.pSetLayouts = &ctx->vk_both_ubo_set_layout;

  vk_result =
      vkAllocateDescriptorSets (ctx->vk_device, &info, &heap->vk_model_ubo1);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_pvm_ubo_set_deinit;
  }

  info.pSetLayouts = &ctx->vk_frag_ubo_set_layout;

  vk_result =
      vkAllocateDescriptorSets (ctx->vk_device, &info, &heap->vk_model_ubo2);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_model_ubo1_set_deinit;
  }

  goto out;

error_model_ubo1_set_deinit:
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo1);

error_pvm_ubo_set_deinit:
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_pvm_ubo);

out:
  return code;
}

owl_private void
owl_vk_frame_heap_sets_deinit (struct owl_vk_frame_heap *heap,
                               struct owl_vk_context const *ctx) {
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo2);
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_model_ubo1);
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_pvm_ubo);
}

owl_private void
owl_vk_frame_heap_sets_write (struct owl_vk_frame_heap *heap,
                              struct owl_vk_context const *ctx) {
  VkDescriptorBufferInfo descriptor;
  VkWriteDescriptorSet write;

  descriptor.buffer = heap->vk_buffer;
  descriptor.offset = 0;
  descriptor.range = sizeof (struct owl_pvm_ubo);

  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = NULL;
  write.dstSet = heap->vk_pvm_ubo;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  write.pImageInfo = NULL;
  write.pBufferInfo = &descriptor;
  write.pTexelBufferView = NULL;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  descriptor.range = sizeof (struct owl_model_ubo1);
  write.dstSet = heap->vk_model_ubo1;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  descriptor.range = sizeof (struct owl_model_ubo2);
  write.dstSet = heap->vk_model_ubo2;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);
}

owl_public enum owl_code
owl_vk_frame_heap_init (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx, owl_u64 size) {
  enum owl_code code;

  code = owl_vk_frame_heap_buffer_init (heap, ctx, size);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_frame_heap_memory_init (heap, ctx);
  if (OWL_SUCCESS != code)
    goto error_buffer_deinit;

  code = owl_vk_frame_heap_sets_init (heap, ctx);
  if (OWL_SUCCESS != code)
    goto error_memory_deinit;

  owl_vk_frame_heap_sets_write (heap, ctx);

  goto out;

error_memory_deinit:
  owl_vk_frame_heap_memory_deinit (heap, ctx);

error_buffer_deinit:
  owl_vk_frame_heap_buffer_deinit (heap, ctx);

out:
  return code;
}

owl_public void
owl_vk_frame_heap_deinit (struct owl_vk_frame_heap *heap,
                          struct owl_vk_context const *ctx) {
  owl_vk_frame_heap_sets_deinit (heap, ctx);
  owl_vk_frame_heap_memory_deinit (heap, ctx);
  owl_vk_frame_heap_buffer_deinit (heap, ctx);
}

owl_public void
owl_vk_frame_heap_unmap (struct owl_vk_frame_heap *heap,
                         struct owl_vk_context const *ctx) {
  vkUnmapMemory (ctx->vk_device, heap->vk_memory);
}

owl_public owl_b32
owl_vk_frame_heap_has_enough_space (struct owl_vk_frame_heap const *heap,
                                    owl_u64 size) {
  return (size + heap->offset) <= heap->size;
}

owl_private void
owl_vk_frame_heap_fill_allocation (
    struct owl_vk_frame_heap const *heap,
    struct owl_vk_frame_allocation *allocation) {
  allocation->offset32 = (owl_u32)heap->offset;
  allocation->offset = heap->offset;
  allocation->vk_buffer = heap->vk_buffer;
  allocation->vk_pvm_ubo_set = heap->vk_pvm_ubo;
  allocation->vk_model_ubo1_set = heap->vk_model_ubo1;
  allocation->vk_model_ubo2_set = heap->vk_model_ubo2;
}

owl_public owl_u64
owl_vk_frame_heap_get_offset (struct owl_vk_frame_heap const *heap) {
  return heap->offset;
}

owl_private owl_u64
owl_vk_frame_heap_offset_update (struct owl_vk_frame_heap *heap,
                                 owl_u64 size) {
  owl_u64 const previous = owl_vk_frame_heap_get_offset (heap);
  heap->offset = owl_alignu2 (previous + size, heap->alignment);
  return previous;
}

owl_public void *
owl_vk_frame_heap_unsafe_allocate (
    struct owl_vk_frame_heap *heap, struct owl_vk_context const *ctx,
    owl_u64 size, struct owl_vk_frame_allocation *allocation) {
  owl_u64 offset;

  owl_unused (ctx);

  owl_vk_frame_heap_fill_allocation (heap, allocation);
  offset = owl_vk_frame_heap_offset_update (heap, size);

  return &((owl_byte *)heap->data)[offset];
}

owl_public void
owl_vk_frame_heap_free (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx) {
  owl_unused (ctx);

  heap->offset = 0;
}

owl_public void
owl_vk_frame_heap_unsafe_copy (struct owl_vk_frame_heap *dst,
                               struct owl_vk_frame_heap const *src) {
  dst->data = src->data;
  dst->offset = src->offset;
  dst->size = src->size;
  dst->alignment = src->alignment;
  dst->vk_memory = src->vk_memory;
  dst->vk_buffer = src->vk_buffer;
  dst->vk_pvm_ubo = src->vk_pvm_ubo;
  dst->vk_model_ubo1 = src->vk_model_ubo1;
  dst->vk_model_ubo2 = src->vk_model_ubo2;
}

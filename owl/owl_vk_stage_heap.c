#include "owl_vk_stage_heap.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_im_command_buffer.h"

owl_public enum owl_code
owl_vk_stage_heap_init (struct owl_vk_stage_heap *heap,
                        struct owl_vk_context const *ctx, owl_u64 size) {
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo memory_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (size && "Expected a size bigger than 0");

  heap->in_use = 0;
  heap->size = size;

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
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

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = req.size;
  memory_info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_CPU_ONLY);

  vk_result =
      vkAllocateMemory (ctx->vk_device, &memory_info, NULL, &heap->vk_memory);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_buffer_deinit;
  }

  vk_result =
      vkBindBufferMemory (ctx->vk_device, heap->vk_buffer, heap->vk_memory, 0);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  vk_result =
      vkMapMemory (ctx->vk_device, heap->vk_memory, 0, size, 0, &heap->data);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto error_memory_deinit;
  }

  goto out;

error_memory_deinit:
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);

error_buffer_deinit:
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);

out:
  return code;
}

owl_public void
owl_vk_stage_heap_deinit (struct owl_vk_stage_heap *heap,
                          struct owl_vk_context const *ctx) {
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);
}

owl_public owl_b32
owl_vk_stage_heap_has_enough_space (struct owl_vk_stage_heap *heap,
                                    owl_u64 size) {
  return heap->size > size;
}

owl_private enum owl_code
owl_vk_stage_heap_reserve (struct owl_vk_stage_heap *heap,
                           struct owl_vk_context const *ctx, owl_u64 size) {
  owl_assert (!heap->in_use && "Must be freed before using it again");

  if (heap->in_use)
    return OWL_ERROR_UNKNOWN;

  if (!owl_vk_stage_heap_has_enough_space (heap, size)) {
    enum owl_code code;

    owl_vk_stage_heap_deinit (heap, ctx);

    code = owl_vk_stage_heap_init (heap, ctx, 2 * size);
    if (OWL_SUCCESS != code)
      return code;
  }

  return OWL_SUCCESS;
}

owl_public void *
owl_vk_stage_heap_allocate (struct owl_vk_stage_heap *heap,
                            struct owl_vk_context const *ctx, owl_u64 size,
                            struct owl_vk_stage_allocation *alloc) {
  owl_byte *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  if (heap->in_use)
    return NULL;

  code = owl_vk_stage_heap_reserve (heap, ctx, size);
  if (OWL_SUCCESS != code)
    return NULL;

  data = heap->data;
  alloc->vk_buffer = heap->vk_buffer;
  heap->in_use = 1;

  return data;
}

owl_public void
owl_vk_stage_heap_free (struct owl_vk_stage_heap *heap,
                        struct owl_vk_context const *ctx) {
  owl_unused (ctx);
  owl_assert (heap->in_use && "Must've allocated something before freeing");
  heap->in_use = 0;
}

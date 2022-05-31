#include "owl_vk_garbage.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage *garbage,
                     struct owl_vk_context const *ctx) {
  owl_unused (ctx);

  garbage->heap_count = 0;

  return OWL_SUCCESS;
}

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage *garbage,
                       struct owl_vk_context const *ctx) {
  owl_i32 i;

  owl_assert (0 <= garbage->heap_count);

  for (i = 0; i < garbage->heap_count; ++i)
    owl_vk_frame_heap_deinit (&garbage->heaps[i], ctx);
}

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame) {
  struct owl_vk_frame_heap const *src;
  struct owl_vk_frame_heap *dst;

  if (OWL_VK_GARBAGE_MAX_HEAP_COUNT <= garbage->heap_count)
    return OWL_ERROR_UNKNOWN;

  src = &frame->heap;
  dst = &garbage->heaps[garbage->heap_count++];

  owl_vk_frame_heap_unsafe_copy (dst, src);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame) {
  struct owl_vk_frame_heap const *src;
  struct owl_vk_frame_heap *dst;

  if (0 >= garbage->heap_count)
    return OWL_ERROR_UNKNOWN;

  src = &garbage->heaps[--garbage->heap_count];
  dst = &frame->heap;

  owl_vk_frame_heap_unsafe_copy (dst, src);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage *garbage,
                      struct owl_vk_context const *ctx) {
  owl_vk_garbage_deinit (garbage, ctx);
  return owl_vk_garbage_init (garbage, ctx);
}

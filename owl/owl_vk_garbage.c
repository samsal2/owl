#include "owl_vk_garbage.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage *garbage,
                     struct owl_vk_context const *ctx) {
  enum owl_code code = OWL_SUCCESS;

  owl_unused (ctx);

  garbage->heap_count = 0;

  return code;
}

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage *garbage,
                       struct owl_vk_context const *ctx) {
  owl_i32 i;
  for (i = 0; i < garbage->heap_count; ++i)
    owl_vk_frame_heap_deinit (&garbage->heaps[i], ctx);
}

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame) {
  if (OWL_VK_GARBAGE_MAX_HEAP_COUNT <= garbage->heap_count)
    return OWL_ERROR_UNKNOWN;

  owl_vk_frame_heap_unsafe_copy (&garbage->heaps[garbage->heap_count++],
                                 &frame->heap);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame) {
  if (0 >= garbage->heap_count)
    return OWL_ERROR_UNKNOWN;

  owl_vk_frame_heap_unsafe_copy (&frame->heap,
                                 &garbage->heaps[--garbage->heap_count]);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage *garbage,
                      struct owl_vk_context const *ctx) {
  owl_vk_garbage_deinit (garbage, ctx);
  return owl_vk_garbage_init (garbage, ctx);
}

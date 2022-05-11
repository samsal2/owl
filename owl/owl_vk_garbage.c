#include "owl_vk_garbage.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"

owl_public void
owl_vk_frame_garbage_init (struct owl_vk_frame_garbage *garbage,
                           struct owl_vk_frame const *frame)
{
  garbage->heap.data = frame->heap.data;
  garbage->heap.alignment = frame->heap.alignment;
  garbage->heap.offset = frame->heap.offset;
  garbage->heap.size = frame->heap.size;
  garbage->heap.vk_buffer = frame->heap.vk_buffer;
  garbage->heap.vk_memory = frame->heap.vk_memory;
  garbage->heap.vk_pvm_ubo_set = frame->heap.vk_pvm_ubo_set;
  garbage->heap.vk_model_ubo1_set = frame->heap.vk_model_ubo1_set;
}

owl_public void
owl_vk_frame_garbage_deinit (struct owl_vk_frame_garbage *garbage,
                             struct owl_vk_context const *ctx)
{
  owl_vk_frame_heap_deinit (&garbage->heap, ctx);
}

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage *garbage,
                     struct owl_vk_context const *ctx)
{
  enum owl_code code = OWL_SUCCESS;

  owl_unused (ctx);

  garbage->frame_count = 0;

  return code;
}

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage *garbage,
                       struct owl_vk_context const *ctx)
{
  owl_i32 i;
  for (i = 0; i < garbage->frame_count; ++i)
    owl_vk_frame_garbage_deinit (&garbage->frames[i], ctx);
}

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame)
{
  if (OWL_VK_GARBAGE_MAX_FRAME_COUNT <= garbage->frame_count)
    return OWL_ERROR_UNKNOWN;

  owl_vk_frame_garbage_init (&garbage->frames[garbage->frame_count++], frame);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame *frame)
{

  struct owl_vk_frame_garbage *frame_garbage;

  enum owl_code code = OWL_SUCCESS;

  if (!garbage->frame_count) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  --garbage->frame_count;

  frame_garbage = &garbage->frames[garbage->frame_count + 1];

  frame->heap.data = frame_garbage->heap.data;
  frame->heap.alignment = frame_garbage->heap.alignment;
  frame->heap.offset = frame_garbage->heap.offset;
  frame->heap.size = frame_garbage->heap.size;
  frame->heap.vk_buffer = frame_garbage->heap.vk_buffer;
  frame->heap.vk_memory = frame_garbage->heap.vk_memory;
  frame->heap.vk_pvm_ubo_set = frame_garbage->heap.vk_pvm_ubo_set;
  frame->heap.vk_model_ubo1_set = frame_garbage->heap.vk_model_ubo1_set;

out:
  return code;
}

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage *garbage,
                      struct owl_vk_context const *ctx)
{
  owl_vk_garbage_deinit (garbage, ctx);
  return owl_vk_garbage_init (garbage, ctx);
}

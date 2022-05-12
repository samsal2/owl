#include "owl_vk_garbage.h"

#include "owl_internal.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"

owl_public void
owl_vk_frame_garbage_init (struct owl_vk_frame_garbage *frame_garbage,
                           struct owl_vk_frame const   *frame)
{
  owl_vk_frame_heap_unsafe_copy (&frame_garbage->heap, &frame->heap);
}

owl_public void
owl_vk_frame_garbage_deinit (struct owl_vk_frame_garbage *frame_garbage,
                             struct owl_vk_context const *ctx)
{
  owl_vk_frame_heap_deinit (&frame_garbage->heap, ctx);
}

owl_public enum owl_code
owl_vk_garbage_init (struct owl_vk_garbage       *garbage,
                     struct owl_vk_context const *ctx)
{
  enum owl_code code = OWL_SUCCESS;

  owl_unused (ctx);

  garbage->frame_count = 0;

  return code;
}

owl_public void
owl_vk_garbage_deinit (struct owl_vk_garbage       *garbage,
                       struct owl_vk_context const *ctx)
{
  owl_i32 i;
  for (i = 0; i < garbage->frame_count; ++i)
    owl_vk_frame_garbage_deinit (&garbage->frames[i], ctx);
}

owl_public enum owl_code
owl_vk_garbage_add_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame   *frame)
{
  if (OWL_VK_GARBAGE_MAX_FRAME_COUNT <= garbage->frame_count)
    return OWL_ERROR_UNKNOWN;

  owl_vk_frame_garbage_init (&garbage->frames[garbage->frame_count++], frame);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_pop_frame (struct owl_vk_garbage *garbage,
                          struct owl_vk_frame   *frame)
{

  struct owl_vk_frame_garbage *frame_garbage;

  if (0 >= garbage->frame_count)
    return OWL_ERROR_UNKNOWN;

  frame_garbage = &garbage->frames[--garbage->frame_count];

  owl_vk_frame_heap_unsafe_copy (&frame->heap, &frame_garbage->heap);

  return OWL_SUCCESS;
}

owl_public enum owl_code
owl_vk_garbage_clear (struct owl_vk_garbage       *garbage,
                      struct owl_vk_context const *ctx)
{
  owl_vk_garbage_deinit (garbage, ctx);
  return owl_vk_garbage_init (garbage, ctx);
}

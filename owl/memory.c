#include <owl/internal.h>
#include <owl/memory.h>
#include <owl/renderer_internal.h>

void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref) {
  OwlByte *data;
  OwlU32 const active = renderer->active_buf;

  if (OWL_SUCCESS != owl_reserve_dyn_buf_mem(renderer, size)) {
    data = NULL;
    goto end;
  }

  ref->offset32 = (OwlU32)renderer->dyn_offsets[active];
  ref->offset = renderer->dyn_offsets[active];
  ref->buf = renderer->dyn_bufs[active];
  ref->set = renderer->dyn_sets[active];

  data = (OwlByte *)renderer->dyn_data +
         (unsigned)active * renderer->dyn_aligned_size +
         renderer->dyn_offsets[active];

  renderer->dyn_offsets[active] = OWL_ALIGN(
      renderer->dyn_offsets[active] + size, renderer->dyn_alignment);
end:
  return data;
}

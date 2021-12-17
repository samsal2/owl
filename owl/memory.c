#include <owl/renderer_internal.h>
#include <owl/internal.h>
#include <owl/memory.h>

void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref) {
  OwlByte *data;
  int const active = renderer->dyn_buf.active;

  if (OWL_SUCCESS != owl_reserve_dyn_buf_mem(renderer, size)) {
    data = NULL;
    goto end;
  }

  ref->offset32 = (OwlU32)renderer->dyn_buf.offsets[active];
  ref->offset = renderer->dyn_buf.offsets[active];
  ref->buf = renderer->dyn_buf.bufs[active];
  ref->set = renderer->dyn_buf.sets[active];

  data = (OwlByte *)renderer->dyn_buf.data +
         (unsigned)active * renderer->dyn_buf.aligned_size +
         renderer->dyn_buf.offsets[active];

  renderer->dyn_buf.offsets[active] = OWL_ALIGN(
      renderer->dyn_buf.offsets[active] + size, renderer->dyn_buf.alignment);
end:
  return data;
}

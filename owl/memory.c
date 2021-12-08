#include "owl/renderer.inl"
#include <owl/internal.h>
#include <owl/memory.h>

void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref) {
  OwlByte *data;
  int const active = renderer->dbl_buf.active;

  if (OWL_SUCCESS != owl_reseve_dbl_buf_mem(renderer, size)) {
    data = NULL;
    goto end;
  }

  ref->offset32 = (OwlU32)renderer->dbl_buf.offsets[active];
  ref->offset = renderer->dbl_buf.offsets[active];
  ref->buf = renderer->dbl_buf.bufs[active];
  ref->set = renderer->dbl_buf.sets[active];

  data = (OwlByte *)renderer->dbl_buf.data +
         (unsigned)active * renderer->dbl_buf.aligned_size +
         renderer->dbl_buf.offsets[active];

  renderer->dbl_buf.offsets[active] = OWL_ALIGN(
      renderer->dbl_buf.offsets[active] + size, renderer->dbl_buf.alignment);
end:
  return data;
}

#include "owl/renderer.inl"
#include <owl/internal.h>
#include <owl/memory.h>

void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref) {
  OwlByte *data;
  int const active = renderer->dbl_buff.active;

  if (OWL_SUCCESS != owl_reserve_dbl_buff_mem(renderer, size)) {
    data = NULL;
    goto end;
  }

  ref->offset32 = (OwlU32)renderer->dbl_buff.offsets[active];
  ref->offset = renderer->dbl_buff.offsets[active];
  ref->buff = renderer->dbl_buff.buffs[active];
  ref->set = renderer->dbl_buff.sets[active];

  data = (OwlByte *)renderer->dbl_buff.data +
         (unsigned)active * renderer->dbl_buff.aligned_size +
         renderer->dbl_buff.offsets[active];

  renderer->dbl_buff.offsets[active] =
      OWL_ALIGN(renderer->dbl_buff.offsets[active] + size,
                  renderer->dbl_buff.alignment);
end:
  return data;
}

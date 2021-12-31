#include "dyn_buf.h"

#include "internal.h"
#include "vk_renderer.h"

void *owl_dyn_buf_alloc(struct owl_vk_renderer *renderer, VkDeviceSize size,
                        struct owl_dyn_buf_ref *ref) {
  OwlByte *data;
  OwlU32 const active = renderer->dyn_active_buf;

  if (OWL_SUCCESS != owl_reserve_dyn_buf_mem(renderer, size)) {
    data = NULL;
    goto end;
  }

  ref->offset32 = (OwlU32)renderer->dyn_offsets[active];
  ref->offset = renderer->dyn_offsets[active];
  ref->buf = renderer->dyn_bufs[active];
  ref->pvm_set = renderer->dyn_pvm_sets[active];

  data = renderer->dyn_data[active] + renderer->dyn_offsets[active];

  renderer->dyn_offsets[active] = OWL_ALIGN(
      renderer->dyn_offsets[active] + size, renderer->dyn_alignment);
end:
  return data;
}

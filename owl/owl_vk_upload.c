
#include "owl_vk_upload.h"

#include "owl_vk_renderer.h"

#include "owl_internal.h"

owl_public owl_code
owl_vk_upload_reserve(struct owl_vk_renderer *vk, uint64_t size) {
  if (vk->upload_buffer_in_use)
    return OWL_ERROR_FATAL;

  if (vk->upload_buffer_size < size) {
    owl_code code;

    /* FIXME (samuel): init first */
    owl_vk_renderer_deinit_upload_buffer(vk);

    code = owl_vk_renderer_init_upload_buffer(vk, size * 2);
    if (code)
      return code;
  }

  return OWL_OK;
}

owl_public void *
owl_vk_upload_alloc(struct owl_vk_renderer *vk, uint64_t size,
                    struct owl_vk_upload_allocation *alloc) {
  owl_code code;

  if (vk->upload_buffer_in_use)
    return NULL;

  code = owl_vk_upload_reserve(vk, size);
  if (code)
    return NULL;

  vk->upload_buffer_in_use = 1;
  alloc->buffer = vk->upload_buffer;

  return vk->upload_buffer_data;
}

owl_public void
owl_vk_upload_free(struct owl_vk_renderer *vk, void *ptr) {
  owl_unused(ptr);
  owl_assert(ptr == vk->upload_buffer_data);
  vk->upload_buffer_in_use = 0;
}

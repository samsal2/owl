#ifndef OWL_MEMORY_H_
#define OWL_MEMORY_H_

#include <owl/renderer_internal.h>

struct owl_tmp_submit_mem_ref {
  OwlU32 offset32; /* for uniform usage */
  OwlDeviceSize offset;
  VkBuffer buf;
  VkDescriptorSet pvm_set;
};

/* valid until next frame, no need to free
 * valid for staging, uniform, vertex and index usage
 */
void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref);

#endif

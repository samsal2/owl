#ifndef OWL_MEMORY_H_
#define OWL_MEMORY_H_

#include <owl/vkutil.h>
#include <vulkan/vulkan.h>

struct owl_tmp_submit_mem_ref {
  OwlU32 offset32; /* for uniform usage */
  OwlDeviceSize offset;
  VkBuffer buff;
  VkDescriptorSet set;
};

/* valid until next frame, no need to free
 * valid for staging, uniform, vertex and index usage
 */
void *owl_alloc_tmp_submit_mem(struct owl_renderer *renderer,
                               OwlDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref);

#endif

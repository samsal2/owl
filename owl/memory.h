#ifndef OWL_MEMORY_H_
#define OWL_MEMORY_H_

#include <owl/vk_renderer.h>

enum owl_vk_mem_vis {
  OWL_VK_MEMORY_VIS_CPU_ONLY,
  OWL_VK_MEMORY_VIS_GPU_ONLY,
  OWL_VK_MEMORY_VIS_CPU_TO_GPU
};

struct owl_tmp_submit_mem_ref {
  OwlU32 offset32; /* for uniform usage */
  OwlVkDeviceSize offset;
  VkBuffer buf;
  VkDescriptorSet pvm_set;
};

/* valid until next frame, no need to free */
void *owl_alloc_tmp_submit_mem(struct owl_vk_renderer *renderer,
                               OwlVkDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref);

#define OWL_VK_MEMORY_TYPE_NONE (OwlVkMemoryType) - 1

OwlVkMemoryType owl_vk_find_mem_type(struct owl_vk_renderer const *renderer,
                                     OwlVkMemoryFilter filter,
                                     enum owl_vk_mem_vis vis);

#endif

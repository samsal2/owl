#ifndef OWL_SOURCE_MEMORY_H_
#define OWL_SOURCE_MEMORY_H_

#include "../include/owl/types.h"

#include <vulkan/vulkan.h>

struct owl_vk_renderer;
struct owl_dyn_buf_ref;

enum owl_vk_mem_vis {
  OWL_VK_MEMORY_VIS_CPU_ONLY,
  OWL_VK_MEMORY_VIS_GPU_ONLY,
  OWL_VK_MEMORY_VIS_CPU_TO_GPU
};

#define OWL_VK_MEMORY_TYPE_NONE (OwlU32) - 1

OwlU32 owl_find_vk_mem_type(struct owl_vk_renderer const *renderer,
                            OwlU32 filter, enum owl_vk_mem_vis vis);

#endif

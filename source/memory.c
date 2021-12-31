#include "memory.h"

#include "internal.h"
#include "vk_renderer.h"

OWL_INTERNAL VkMemoryPropertyFlags
owl_vk_as_property_flags(enum owl_vk_mem_vis vis) {
  switch (vis) {
  case OWL_VK_MEMORY_VIS_CPU_ONLY:
  case OWL_VK_MEMORY_VIS_CPU_TO_GPU:
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  case OWL_VK_MEMORY_VIS_GPU_ONLY:
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
}

OwlU32 owl_find_vk_mem_type(struct owl_vk_renderer const *renderer,
                            OwlU32 filter, enum owl_vk_mem_vis vis) {
  OwlU32 i;
  VkMemoryPropertyFlags props = owl_vk_as_property_flags(vis);

  for (i = 0; i < renderer->device_mem_properties.memoryTypeCount; ++i) {
    OwlU32 f = renderer->device_mem_properties.memoryTypes[i].propertyFlags;

    if ((f & props) && (filter & (1U << i)))
      return i;
  }

  return OWL_VK_MEMORY_TYPE_NONE;
}

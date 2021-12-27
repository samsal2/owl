#include <owl/internal.h>
#include <owl/memory.h>
#include <owl/vk_internal.h>

void *owl_alloc_tmp_submit_mem(struct owl_vk_renderer *renderer,
                               OwlVkDeviceSize size,
                               struct owl_tmp_submit_mem_ref *ref) {
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

OwlVkMemoryType owl_vk_find_mem_type(struct owl_vk_renderer const *renderer,
                                     OwlVkMemoryFilter filter,
                                     enum owl_vk_mem_vis vis) {
  OwlVkMemoryType i;
  VkMemoryPropertyFlags props = owl_vk_as_property_flags(vis);

  for (i = 0; i < renderer->device_mem_properties.memoryTypeCount; ++i) {
    OwlU32 f = renderer->device_mem_properties.memoryTypes[i].propertyFlags;

    if ((f & props) && (filter & (1U << i)))
      return i;
  }

  return OWL_VK_MEMORY_TYPE_NONE;
}

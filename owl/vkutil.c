#include <owl/internal.h>
#include <owl/renderer_internal.h>

#define OWL_MAX_QUEUE_PROPERTIES 16


int owl_vk_validate_extensions(OwlU32 count,
                               VkExtensionProperties const *extensions) {
  char const *validate[] = OWL_VK_DEVICE_EXTENSIONS;
  OwlU32 i, j, found[OWL_ARRAY_SIZE(validate)];

  OWL_MEMSET(found, 0, sizeof(found));

  for (i = 0; i < count; ++i)
    for (j = 0; j < OWL_ARRAY_SIZE(validate); ++j)
      if (!strcmp(validate[j], extensions[i].extensionName))
        found[j] = 1;

  for (i = 0; i < OWL_ARRAY_SIZE(validate); ++i)
    if (!found[i])
      return 0;

  return 1;
}

int owl_vk_shares_families(struct owl_renderer const *renderer) {
  return renderer->graphics_family == renderer->present_family;
}

OwlU32 owl_vk_queue_count(struct owl_renderer const *renderer) {
  return owl_vk_shares_families(renderer) ? 1 : 2;
}

OWL_INTERNAL VkMemoryPropertyFlags
owl_vk_as_property_flags(enum owl_vk_mem_visibility visibility) {
  switch (visibility) {
  case OWL_VK_MEMORY_VISIBILITY_CPU_ONLY:
  case OWL_VK_MEMORY_VISIBILITY_CPU_TO_GPU:
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  case OWL_VK_MEMORY_VISIBILITY_GPU_ONLY:
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
}

OwlVkMemoryType
owl_vk_find_mem_type(struct owl_renderer const *renderer,
                     OwlVkMemoryFilter filter,
                     enum owl_vk_mem_visibility visibility) {
  OwlVkMemoryType type;
  VkMemoryPropertyFlags props = owl_vk_as_property_flags(visibility);

  for (type = 0; type < renderer->device_mem_properties.memoryTypeCount;
       ++type) {
    uint32_t flags =
        renderer->device_mem_properties.memoryTypes[type].propertyFlags;

    if ((flags & props) && (filter & (1U << type)))
      return type;
  }

  return OWL_VK_MEMORY_TYPE_NONE;
}

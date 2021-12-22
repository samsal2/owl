#include <owl/internal.h>
#include <owl/renderer_internal.h>

#define OWL_MAX_QUEUE_PROPERTIES 16

#define OWL_QUEUE_UNSELECTED (OwlU32) - 1
int owl_vk_query_families(struct owl_renderer const *renderer,
                          VkPhysicalDevice device, OwlU32 *graphics_family,
                          OwlU32 *present_family) {
  OwlU32 i;
  OwlU32 count;
  VkQueueFamilyProperties props[OWL_MAX_QUEUE_PROPERTIES];

  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
  OWL_ASSERT(OWL_MAX_QUEUE_PROPERTIES > count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props);

  *graphics_family = OWL_QUEUE_UNSELECTED;
  *present_family = OWL_QUEUE_UNSELECTED;

  for (i = 0; i < count; ++i) {
    VkBool32 surface;

    if (!props[i].queueCount)
      continue;

    if (VK_QUEUE_GRAPHICS_BIT & props[i].queueFlags)
      *graphics_family = i;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
        device, i, renderer->surface, &surface));

    if (surface)
      *present_family = i;

    if (OWL_QUEUE_UNSELECTED == *graphics_family)
      continue;

    if (OWL_QUEUE_UNSELECTED == *present_family)
      continue;

    return 1;
  }

  return 0;
}
#undef OWL_QUEUE_UNSELECTED

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

OtterVkMemoryType
owl_vk_find_mem_type(struct owl_renderer const *renderer,
                     OtterVkMemoryFilter filter,
                     enum owl_vk_mem_visibility visibility) {
  OtterVkMemoryType type;
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

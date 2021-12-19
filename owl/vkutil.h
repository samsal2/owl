#ifndef OWL_VKUTIL_H_
#define OWL_VKUTIL_H_

#include <owl/fwd.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

#ifndef NDEBUG
#define OWL_VK_CHECK(e)                                                      \
  do {                                                                       \
    VkResult const result_ = e;                                              \
    OWL_ASSERT(VK_SUCCESS == result_);                                       \
  } while (0)
#else
#define OWL_VK_CHECK(e) e
#endif

/* clang-format off */
#define OWL_VK_DEVICE_EXTENSIONS { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
/* clang-format on */
#define OWL_MB (1024 * 1024)
#define OWL_VK_MEMORY_TYPE_NONE (OwlU32) - 1
#define OWL_ALIGN(v, alignment) ((v + alignment - 1) & ~(alignment - 1))

typedef OwlU32 OtterVkMemoryType;
typedef OwlU32 OtterVkMemoryFilter;
typedef OwlU32 OtterVkQueueFamily;
typedef VkDeviceSize OwlDeviceSize;

enum owl_vk_queue_type {
  OWL_VK_QUEUE_TYPE_GRAPHICS,
  OWL_VK_QUEUE_TYPE_PRESENT,
  OWL_VK_QUEUE_TYPE_COUNT
};

enum owl_vk_mem_visibility {
  OWL_VK_MEMORY_VISIBILITY_CPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_GPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_CPU_TO_GPU
};

int owl_vk_query_families(
    struct owl_renderer const *renderer, VkPhysicalDevice device,
    OtterVkQueueFamily families[OWL_VK_QUEUE_TYPE_COUNT]);

int owl_vk_validate_extensions(OwlU32 count,
                               VkExtensionProperties const *extensions);

int owl_vk_shares_families(struct owl_renderer const *renderer);

int owl_vk_queue_count(struct owl_renderer const *renderer);

OtterVkMemoryType owl_vk_find_mem_type(struct owl_renderer const *renderer,
                                       OtterVkMemoryFilter filter,
                                       enum owl_vk_mem_visibility visibility);

#endif

#ifndef OWL_VK_UPLOAD_H_
#define OWL_VK_UPLOAD_H_

#include "owl-definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_renderer;

struct owl_vk_upload_allocation {
  VkBuffer buffer;
};

owl_public owl_code
owl_vk_upload_reserve(struct owl_vk_renderer *vk, uint64_t size);

owl_public void *
owl_vk_upload_alloc(struct owl_vk_renderer *vk, uint64_t size,
                    struct owl_vk_upload_allocation *alloc);

owl_public void
owl_vk_upload_free(struct owl_vk_renderer *vk, void *ptr);

OWL_END_DECLS

#endif

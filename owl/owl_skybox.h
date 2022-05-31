#ifndef OWL_SKYBOX_H_
#define OWL_SKYBOX_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_stage_heap;

struct owl_skybox
{
  owl_i32         width;
  owl_i32         height;
  VkImage         vk_image;
  VkDeviceMemory  vk_memory;
  VkImageView     vk_image_view;
  VkDescriptorSet vk_set;
  VkSampler       vk_sampler;
};

owl_public enum owl_code
owl_skybox_init (struct owl_skybox           *sb,
                 struct owl_vk_context const *ctx,
                 struct owl_vk_stage_heap    *heap,
                 char const                  *path);

owl_public void
owl_skybox_deinit (struct owl_skybox *sb, struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

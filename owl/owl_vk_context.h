#ifndef OWL_VK_CONTEXT_H_
#define OWL_VK_CONTEXT_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_window;

enum owl_memory_properties {
  OWL_MEMORY_PROPERTIES_CPU_ONLY,
  OWL_MEMORY_PROPERTIES_GPU_ONLY
};

#define OWL_VK_CONTEXT_MAX_DEVICE_OPTION_COUNT 8
struct owl_vk_context {
  VkInstance vk_instance;
  VkDebugUtilsMessengerEXT vk_debug_messenger;

  VkSurfaceKHR vk_surface;
  VkSurfaceFormatKHR vk_surface_format;

  VkPhysicalDevice vk_physical_device;
  VkDevice vk_device;

  owl_u32 graphics_queue_family;
  owl_u32 present_queue_family;
  VkQueue vk_graphics_queue;
  VkQueue vk_present_queue;

  VkSampleCountFlagBits msaa;
  VkPresentModeKHR vk_present_mode;

  VkFormat vk_depth_stencil_format;
  VkRenderPass vk_main_render_pass;

  VkDescriptorPool vk_set_pool;
  VkCommandPool vk_command_pool;

  VkDescriptorSetLayout vk_vert_ubo_set_layout;
  VkDescriptorSetLayout vk_frag_ubo_set_layout;
  VkDescriptorSetLayout vk_both_ubo_set_layout;
  VkDescriptorSetLayout vk_vert_ssbo_set_layout;
  VkDescriptorSetLayout vk_frag_image_set_layout;

  owl_u32 vk_device_option_count;
  VkPhysicalDevice vk_device_options[OWL_VK_CONTEXT_MAX_DEVICE_OPTION_COUNT];
};

owl_public enum owl_code
owl_vk_context_init (struct owl_vk_context *ctx, struct owl_window const *w);

owl_public void
owl_vk_context_deinit (struct owl_vk_context *ctx);

owl_public owl_u32
owl_vk_context_get_memory_type (struct owl_vk_context const *ctx,
                                owl_u32 filter,
                                enum owl_memory_properties props);

owl_public enum owl_code
owl_vk_context_device_wait (struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_context_graphics_queue_wait (struct owl_vk_context const *ctx);

OWL_END_DECLS

#endif

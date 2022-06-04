#ifndef OWL_VK_RENDERER_H_
#define OWL_VK_RENDERER_H_

#include "owl-definitions.h"
#include "owl-vk-font.h"
#include "owl-vk-pipeline.h"
#include "owl-vk-texture.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_MAX_GARBAGE_ITEMS 8
#define OWL_FIRST_CHAR ((int32_t)(' '))
#define OWL_NUM_CHARS ((int32_t)('~' - ' '))

#define OWL_DEF_FRAME_GARBAGE_ARRAY(type, name)                               \
  type name[OWL_MAX_SWAPCHAIN_IMAGES][OWL_MAX_GARBAGE_ITEMS]

struct owl_plataform;

struct owl_vk_renderer {
  struct owl_plataform *plataform;

  owl_m4 projection;
  owl_m4 view;

  int32_t width;
  int32_t height;

  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;

  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;

  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t graphics_queue_family;
  uint32_t present_queue_family;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSampleCountFlagBits msaa;

  VkImage color_image;
  VkDeviceMemory color_memory;
  VkImageView color_image_view;

  VkFormat depth_format;
  VkImage depth_image;
  VkDeviceMemory depth_mem;
  VkImageView depth_image_view;

  VkRenderPass main_render_pass;

  VkCommandBuffer im_command_buffer;

  VkPresentModeKHR present_mode;
  VkSwapchainKHR swapchain;
  uint32_t swapchain_width;
  uint32_t swapchain_height;
  uint32_t swapchain_image;
  uint32_t num_swapchain_images;
  VkImage swapchain_images[OWL_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_image_views[OWL_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer swapchain_framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];
  VkClearValue swapchain_clear_values[2];

  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;

  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule text_fragment_shader;
  VkShaderModule model_vertex_shader;
  VkShaderModule model_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;

  VkDescriptorSetLayout ubo_vertex_descriptor_set_layout;
  VkDescriptorSetLayout ubo_fragment_descriptor_set_layout;
  VkDescriptorSetLayout ubo_both_descriptor_set_layout;
  VkDescriptorSetLayout ssbo_vertex_descriptor_set_layout;
  VkDescriptorSetLayout image_fragment_descriptor_set_layout;
  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout model_pipeline_layout;

  enum owl_vk_pipeline pipeline;
  VkPipeline pipelines[OWL_VK_NUM_PIPELINES];
  VkPipelineLayout pipeline_layouts[OWL_VK_NUM_PIPELINES];

  void *upload_heap_data;
  VkDeviceSize upload_heap_size;
  VkBuffer upload_heap_buffer;
  VkDeviceMemory upload_heap_memory;
  int32_t upload_heap_in_use;

  VkSampler linear_sampler;

  int32_t skybox_loaded;
  VkImage skybox_image;
  VkDeviceMemory skybox_memory;
  VkImageView skybox_image_view;
  VkDescriptorSet skybox_descriptor_set;

  int32_t font_loaded;
  struct owl_vk_texture font_atlas;
  struct owl_vk_packed_char font_chars[OWL_NUM_CHARS];

  VkCommandPool frame_command_pools[OWL_MAX_SWAPCHAIN_IMAGES];
  VkCommandBuffer frame_command_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  VkFence frame_in_flight_fences[OWL_MAX_SWAPCHAIN_IMAGES];
  VkSemaphore frame_image_available_semaphores[OWL_MAX_SWAPCHAIN_IMAGES];
  VkSemaphore frame_render_done_semaphores[OWL_MAX_SWAPCHAIN_IMAGES];

  uint32_t frame;
  VkDeviceSize frame_heap_size;
  VkDeviceSize frame_heap_offset;
  VkDeviceSize frame_heap_alignment;

  void *frame_heap_data[OWL_MAX_SWAPCHAIN_IMAGES];
  VkBuffer frame_heap_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  VkDeviceMemory frame_heap_memories[OWL_MAX_SWAPCHAIN_IMAGES];
  VkDescriptorSet frame_heap_pvm_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  VkDescriptorSet frame_heap_model1_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  VkDescriptorSet frame_heap_model2_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];

  uint32_t num_frame_garbage_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkBuffer, frame_garbage_buffers);

  uint32_t num_frame_garbage_memories[OWL_MAX_SWAPCHAIN_IMAGES];
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkDeviceMemory, frame_garbage_memories);

  uint32_t num_frame_garbage_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkDescriptorSet, frame_garbage_descriptor_sets);

  PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
  PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
};

owl_public owl_code
owl_vk_renderer_init(struct owl_vk_renderer *vk,
                     struct owl_plataform *plataform);

owl_public void
owl_vk_renderer_deinit(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

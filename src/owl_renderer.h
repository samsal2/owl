#ifndef OWL_RENDERER_H
#define OWL_RENDERER_H

#include "owl_font.h"
#include "owl_texture.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_NUM_IN_FLIGHT_FRAMES 3
#define OWL_NUM_GARBAGE_FRAMES (OWL_NUM_IN_FLIGHT_FRAMES + 1)

struct owl_renderer_upload_allocation {
  VkBuffer buffer;
};

struct owl_renderer_vertex_allocation {
  uint64_t offset;
  VkBuffer buffer;
};

struct owl_renderer_index_allocation {
  uint64_t offset;
  VkBuffer buffer;
};

struct owl_renderer_uniform_allocation {
  uint32_t offset;
  VkBuffer buffer;
  VkDescriptorSet common_descriptor_set;
  VkDescriptorSet model_descriptor_set;
};

struct owl_renderer {
  struct owl_plataform *plataform;

  owl_m4 projection;
  owl_m4 view;

  uint32_t width;
  uint32_t height;

  VkClearValue clear_values[2];

  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;

  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;

  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t graphics_family;
  uint32_t present_family;
  uint32_t compute_family;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue compute_queue;

  VkSampleCountFlagBits msaa;
  VkFormat depth_format;

  VkImage color_image;
  VkDeviceMemory color_memory;
  VkImageView color_image_view;

  VkImage depth_image;
  VkDeviceMemory depth_memory;
  VkImageView depth_image_view;

  VkRenderPass main_render_pass;

  VkCommandBuffer im_command_buffer;

  VkPresentModeKHR present_mode;

  VkSwapchainKHR swapchain;
  uint32_t swapchain_image;
  uint32_t num_swapchain_images;
  VkImage swapchain_images[OWL_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_image_views[OWL_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer swapchain_framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];

  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;

  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule text_fragment_shader;
  VkShaderModule model_vertex_shader;
  VkShaderModule model_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;
  VkShaderModule fluid_simulation_advect_shader;
  VkShaderModule fluid_simulation_curl_shader;
  VkShaderModule fluid_simulation_diverge_shader;
  VkShaderModule fluid_simulation_gradient_subtract_shader;
  VkShaderModule fluid_simulation_pressure_shader;
  VkShaderModule fluid_simulation_splat_shader;
  VkShaderModule fluid_simulation_vorticity_shader;

  VkDescriptorSetLayout common_uniform_descriptor_set_layout;
  VkDescriptorSetLayout common_texture_descriptor_set_layout;
  VkDescriptorSetLayout model_uniform_descriptor_set_layout;
  VkDescriptorSetLayout model_joints_descriptor_set_layout;
  VkDescriptorSetLayout model_material_descriptor_set_layout;
  VkDescriptorSetLayout fluid_simulation_uniform_descriptor_set_layout;
  VkDescriptorSetLayout fluid_simulation_source_descriptor_set_layout;

  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout model_pipeline_layout;
  VkPipelineLayout fluid_simulation_pipeline_layout;

  VkPipeline basic_pipeline;
  VkPipeline wires_pipeline;
  VkPipeline text_pipeline;
  VkPipeline model_pipeline;
  VkPipeline skybox_pipeline;

  VkPipeline fluid_simulation_advect_pipeline;
  VkPipeline fluid_simulation_curl_pipeline;
  VkPipeline fluid_simulation_diverge_pipeline;
  VkPipeline fluid_simulation_gradient_subtract_pipeline;
  VkPipeline fluid_simulation_pressure_pipeline;
  VkPipeline fluid_simulation_splat_pipeline;
  VkPipeline fluid_simulation_vorticity_pipeline;

  VkSampler linear_sampler;

  int32_t skybox_loaded;
  struct owl_texture skybox;

  int32_t font_loaded;
  struct owl_font font;

  int32_t upload_buffer_in_use;
  void *upload_buffer_data;
  VkDeviceSize upload_buffer_size;
  VkBuffer upload_buffer;
  VkDeviceMemory upload_buffer_memory;

  uint32_t frame;
  uint32_t num_frames;

  VkCommandPool submit_command_pools[OWL_NUM_IN_FLIGHT_FRAMES];
  VkCommandBuffer submit_command_buffers[OWL_NUM_IN_FLIGHT_FRAMES];

  VkFence in_flight_fences[OWL_NUM_IN_FLIGHT_FRAMES];
  VkSemaphore acquire_semaphores[OWL_NUM_IN_FLIGHT_FRAMES];
  VkSemaphore render_done_semaphores[OWL_NUM_IN_FLIGHT_FRAMES];

  VkDeviceSize vertex_buffer_size;
  VkDeviceSize vertex_buffer_offset;
  VkDeviceSize vertex_buffer_alignment;
  VkDeviceSize vertex_buffer_aligned_size;
  VkDeviceMemory vertex_buffer_memory;
  void *vertex_buffer_data;
  VkBuffer vertex_buffers[OWL_NUM_IN_FLIGHT_FRAMES];

  VkDeviceSize index_buffer_size;
  VkDeviceSize index_buffer_offset;
  VkDeviceSize index_buffer_alignment;
  VkDeviceMemory index_buffer_memory;
  VkDeviceSize index_buffer_aligned_size;
  void *index_buffer_data;
  VkBuffer index_buffers[OWL_NUM_IN_FLIGHT_FRAMES];

  VkDeviceSize uniform_buffer_size;
  VkDeviceSize uniform_buffer_offset;
  VkDeviceSize uniform_buffer_alignment;
  VkDeviceMemory uniform_buffer_memory;
  VkDeviceSize uniform_buffer_aligned_size;
  void *uniform_buffer_data;
  VkBuffer uniform_buffers[OWL_NUM_IN_FLIGHT_FRAMES];
  VkDescriptorSet uniform_pvm_descriptor_sets[OWL_NUM_IN_FLIGHT_FRAMES];
  VkDescriptorSet uniform_model_descriptor_sets[OWL_NUM_IN_FLIGHT_FRAMES];

  uint32_t garbage;
  uint32_t num_garbage_buffers[OWL_NUM_GARBAGE_FRAMES];
  uint32_t num_garbage_memories[OWL_NUM_GARBAGE_FRAMES];
  uint32_t num_garbage_descriptor_sets[OWL_NUM_GARBAGE_FRAMES];

  VkBuffer garbage_buffers[OWL_NUM_GARBAGE_FRAMES][32];
  VkDeviceMemory garbage_memories[OWL_NUM_GARBAGE_FRAMES][32];
  VkDescriptorSet garbage_descriptor_sets[OWL_NUM_GARBAGE_FRAMES][32];

  PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
  PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
  PFN_vkDebugMarkerSetObjectNameEXT vk_debug_marker_set_object_name_ext;
};

OWLAPI int owl_renderer_init(struct owl_renderer *r, struct owl_plataform *p);

OWLAPI void owl_renderer_deinit(struct owl_renderer *r);

OWLAPI int owl_renderer_update_dimensions(struct owl_renderer *r);

OWLAPI int owl_renderer_begin_frame(struct owl_renderer *r);

OWLAPI int owl_renderer_end_frame(struct owl_renderer *r);

OWLAPI void *owl_renderer_vertex_allocate(
    struct owl_renderer *r, uint64_t size,
    struct owl_renderer_vertex_allocation *allocation);

OWLAPI void *
owl_renderer_index_allocate(struct owl_renderer *r, uint64_t size,
                            struct owl_renderer_index_allocation *allocation);

OWLAPI void *
owl_renderer_uniform_allocate(struct owl_renderer *r, uint64_t size,
                              struct owl_renderer_uniform_allocation *alloc);

OWLAPI void *
owl_renderer_upload_allocate(struct owl_renderer *r, uint64_t size,
                             struct owl_renderer_upload_allocation *alloc);

OWLAPI void owl_renderer_upload_free(struct owl_renderer *r, void *ptr);

OWLAPI int owl_renderer_load_font(struct owl_renderer *r, uint32_t size,
                                  char const *path);

OWLAPI void owl_renderer_unload_font(struct owl_renderer *r);

OWLAPI int owl_renderer_load_skybox(struct owl_renderer *r, char const *path);

OWLAPI void owl_renderer_unload_skybox(struct owl_renderer *r);

OWLAPI uint32_t owl_renderer_find_memory_type(struct owl_renderer *r,
                                              uint32_t filter,
                                              uint32_t properties);

OWLAPI int owl_renderer_begin_im_command_buffer(struct owl_renderer *r);

OWLAPI int owl_renderer_end_im_command_buffer(struct owl_renderer *r);

OWL_END_DECLARATIONS

#endif

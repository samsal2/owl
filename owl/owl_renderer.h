#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "owl_definitions.h"
#include "owl_texture_2d.h"
#include "owl_texture_cube.h"
#include "owl_vector.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

#define OWL_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define OWL_IN_FLIGHT_FRAME_COUNT 3
#define OWL_MAX_GARBAGE_COUNT 8
#define OWL_FONT_FIRST_CHAR ((int)(' '))
#define OWL_FONT_CHAR_COUNT ((int)('~' - ' '))

struct owl_plataform;

struct owl_frame_allocation {
  uint32_t offset32;
  VkDeviceSize offset;
  VkBuffer buffer;
  VkDescriptorSet pvm_set;
  VkDescriptorSet model1_set;
  VkDescriptorSet model2_set;
};

struct owl_upload_allocation {
  VkBuffer buffer;
};

struct owl_packed_char {
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
  float x_offset;
  float y_offset;
  float x_advance;
  float x_offset2;
  float y_offset2;
};

struct owl_glyph {
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

struct owl_renderer {
  struct owl_plataform *plataform;

  owl_m4 projection;
  owl_m4 view;

  uint32_t width;
  uint32_t height;

  VkClearValue clear_values[2];

  VkInstance instance;
  VkDebugUtilsMessengerEXT debug;

  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;

  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t graphics_queue_family;
  uint32_t present_queue_family;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSampleCountFlagBits msaa;
  VkFormat depth_format;

  VkImage color_image;
  VkDeviceMemory color_memory;
  VkImageView color_image_view;

  VkImage depth_image;
  VkDeviceMemory depth_memory;
  VkImageView depth_image_view;

  VkRenderPass main_render_pass;
  VkRenderPass offscreen_render_pass;

  VkCommandBuffer im_command_buffer;

  VkPresentModeKHR present_mode;

  VkSwapchainKHR swapchain;
  uint32_t image;
  uint32_t image_count;
  VkImage images[OWL_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkImageView image_views[OWL_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGE_COUNT];

  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;

  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule text_fragment_shader;
  VkShaderModule model_vertex_shader;
  VkShaderModule model_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;

  VkDescriptorSetLayout ubo_vertex_set_layout;
  VkDescriptorSetLayout ubo_fragment_set_layout;
  VkDescriptorSetLayout ubo_both_set_layout;
  VkDescriptorSetLayout ssbo_vertex_set_layout;
  VkDescriptorSetLayout image_fragment_set_layout;

  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout model_pipeline_layout;

  VkPipeline basic_pipeline;
  VkPipeline wires_pipeline;
  VkPipeline text_pipeline;
  VkPipeline model_pipeline;
  VkPipeline skybox_pipeline;

  int32_t upload_buffer_in_use;
  void *upload_buffer_data;
  VkDeviceSize upload_buffer_size;
  VkBuffer upload_buffer;
  VkDeviceMemory upload_buffer_memory;

  VkSampler linear_sampler;

  int32_t skybox_loaded;
  struct owl_texture_cube skybox;

  int32_t font_loaded;
  struct owl_texture_2d font_atlas;
  struct owl_packed_char font_chars[OWL_FONT_CHAR_COUNT];

  uint32_t frame;
  uint32_t frame_count;

  VkCommandPool frame_command_pools[OWL_IN_FLIGHT_FRAME_COUNT];
  VkCommandBuffer frame_command_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
  VkFence frame_in_flight_fences[OWL_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore frame_acquire_semaphores[OWL_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore frame_render_done_semaphores[OWL_IN_FLIGHT_FRAME_COUNT];

  VkDeviceSize render_buffer_size;
  VkDeviceSize render_buffer_alignment;
  VkDeviceSize render_buffer_offset;

  void *render_buffer_data[OWL_IN_FLIGHT_FRAME_COUNT];
  VkBuffer render_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
  VkDeviceMemory render_buffer_memories[OWL_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet render_buffer_pvm_sets[OWL_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet render_buffer_model1_sets[OWL_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet render_buffer_model2_sets[OWL_IN_FLIGHT_FRAME_COUNT];

  owl_vector(VkBuffer) garbage_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
  owl_vector(VkDeviceMemory) garbage_memories[OWL_IN_FLIGHT_FRAME_COUNT];
  owl_vector(VkDescriptorSet) garbage_sets[OWL_IN_FLIGHT_FRAME_COUNT];

  PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
  PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
};

owl_public owl_code
owl_renderer_init(struct owl_renderer *renderer,
                  struct owl_plataform *plataform);

owl_public void
owl_renderer_deinit(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_init_render_buffers(struct owl_renderer *renderer, uint64_t size);

owl_public void
owl_renderer_deinit_render_buffers(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_init_upload_buffer(struct owl_renderer *renderer, uint64_t size);

owl_public void
owl_renderer_deinit_upload_buffer(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_resize_swapchain(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_begin_frame(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_end_frame(struct owl_renderer *renderer);

owl_public void *
owl_renderer_frame_allocate(struct owl_renderer *renderer, uint64_t size,
                            struct owl_frame_allocation *alloc);

owl_public void *
owl_renderer_upload_allocate(struct owl_renderer *renderer, uint64_t size,
                             struct owl_upload_allocation *alloc);

owl_public void
owl_renderer_upload_free(struct owl_renderer *renderer, void *ptr);

owl_public owl_code
owl_renderer_load_font(struct owl_renderer *renderer, uint32_t size,
                       char const *path);

owl_public void
owl_renderer_unload_font(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_load_skybox(struct owl_renderer *renderer, char const *path);

owl_public void
owl_renderer_unload_skybox(struct owl_renderer *renderer);

owl_public uint32_t
owl_renderer_find_memory_type(struct owl_renderer *renderer, uint32_t filter,
                              uint32_t properties);

owl_public owl_code
owl_renderer_begin_im_command_buffer(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_end_im_command_buffer(struct owl_renderer *renderer);

owl_public owl_code
owl_renderer_fill_glyph(struct owl_renderer *renderer, char c, owl_v2 offset,
                        struct owl_glyph *glyph);

OWL_END_DECLS

#endif

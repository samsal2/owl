#ifndef OWL_RENDERER_H
#define OWL_RENDERER_H

#include "owl_definitions.h"
#include "owl_texture.h"
#include "vulkan/vulkan_core.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLARATIONS

#define OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define OWL_RENDERER_IN_FLIGHT_FRAME_COUNT 2
#define OWL_RENDERER_GARBAGE_COUNT 3
#define OWL_RENDERER_FONT_FIRST_CHAR ((int)(' '))
#define OWL_RENDERER_CHAR_COUNT ((int)('~' - ' '))

struct owl_plataform;

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

  VkCommandBuffer immediate_command_buffer;

  VkPresentModeKHR present_mode;

  VkSwapchainKHR swapchain;
  uint32_t swapchain_image;
  uint32_t swapchain_image_count;
  VkImage swapchain_images[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkImageView swapchain_image_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkFramebuffer swapchain_framebuffers[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];

  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;

  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule text_fragment_shader;
  VkShaderModule model_vertex_shader;
  VkShaderModule model_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;

  VkDescriptorSetLayout common_uniform_descriptor_set_layout;
  VkDescriptorSetLayout common_texture_descriptor_set_layout;
  VkDescriptorSetLayout model_uniform_descriptor_set_layout;
  VkDescriptorSetLayout model_joints_descriptor_set_layout;
  VkDescriptorSetLayout model_material_descriptor_set_layout;

  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout model_pipeline_layout;

  VkPipeline basic_pipeline;
  VkPipeline wires_pipeline;
  VkPipeline text_pipeline;
  VkPipeline model_pipeline;
  VkPipeline skybox_pipeline;

  VkSampler linear_sampler;

  int32_t skybox_loaded;
  struct owl_texture skybox;

  int32_t font_loaded;
  struct owl_texture font_atlas;
  struct owl_packed_char font_chars[OWL_RENDERER_CHAR_COUNT];

  int32_t upload_buffer_in_use;
  void *upload_buffer_data;
  VkDeviceSize upload_buffer_size;
  VkBuffer upload_buffer;
  VkDeviceMemory upload_buffer_memory;

  uint32_t frame;
  uint32_t frame_count;

  VkCommandPool submit_command_pools[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkCommandBuffer submit_command_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

  VkFence in_flight_fences[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore acquire_semaphores[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore render_done_semaphores[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

  VkDeviceSize vertex_buffer_size;
  VkDeviceSize vertex_buffer_offset;
  VkDeviceSize vertex_buffer_alignment;
  VkDeviceSize vertex_buffer_aligned_size;
  VkDeviceMemory vertex_buffer_memory;
  void *vertex_buffer_data;
  VkBuffer vertex_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

  VkDeviceSize index_buffer_size;
  VkDeviceSize index_buffer_offset;
  VkDeviceSize index_buffer_alignment;
  VkDeviceMemory index_buffer_memory;
  VkDeviceSize index_buffer_aligned_size;
  void *index_buffer_data;
  VkBuffer index_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

  VkDeviceSize uniform_buffer_size;
  VkDeviceSize uniform_buffer_offset;
  VkDeviceSize uniform_buffer_alignment;
  VkDeviceMemory uniform_buffer_memory;
  VkDeviceSize uniform_buffer_aligned_size;
  void *uniform_buffer_data;
  VkBuffer uniform_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  /* clang-format off */
  VkDescriptorSet uniform_pvm_descriptor_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet uniform_model_descriptor_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  /* clang-format on */

  uint32_t garbage;
  uint32_t garbage_buffer_counts[OWL_RENDERER_GARBAGE_COUNT];
  uint32_t garbage_memory_counts[OWL_RENDERER_GARBAGE_COUNT];
  uint32_t garbage_descriptor_set_counts[OWL_RENDERER_GARBAGE_COUNT];

  VkBuffer garbage_buffers[OWL_RENDERER_GARBAGE_COUNT][32];
  VkDeviceMemory garbage_memories[OWL_RENDERER_GARBAGE_COUNT][32];
  VkDescriptorSet garbage_descriptor_sets[OWL_RENDERER_GARBAGE_COUNT][32];
};

OWL_PUBLIC owl_code
owl_renderer_init(struct owl_renderer *renderer,
                  struct owl_plataform *plataform);

OWL_PUBLIC void
owl_renderer_deinit(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_resize_swapchain(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_begin_frame(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_end_frame(struct owl_renderer *renderer);

OWL_PUBLIC void *
owl_renderer_vertex_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_vertex_allocation *allocation);

OWL_PUBLIC void *
owl_renderer_index_allocate(struct owl_renderer *renderer, uint64_t size,
                            struct owl_renderer_index_allocation *allocation);

OWL_PUBLIC void *
owl_renderer_uniform_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_uniform_allocation *allocation);

OWL_PUBLIC void *
owl_renderer_upload_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_upload_allocation *allocation);

OWL_PUBLIC void
owl_renderer_upload_free(struct owl_renderer *renderer, void *ptr);

OWL_PUBLIC owl_code
owl_renderer_load_font(struct owl_renderer *renderer, uint32_t size,
                       char const *path);

OWL_PUBLIC void
owl_renderer_unload_font(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_load_skybox(struct owl_renderer *renderer, char const *path);

OWL_PUBLIC void
owl_renderer_unload_skybox(struct owl_renderer *renderer);

OWL_PUBLIC uint32_t
owl_renderer_find_memory_type(struct owl_renderer *renderer, uint32_t filter,
                              uint32_t properties);

OWL_PUBLIC owl_code
owl_renderer_begin_immediate_command_buffer(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_end_immediate_command_buffer(struct owl_renderer *renderer);

OWL_PUBLIC owl_code
owl_renderer_fill_glyph(struct owl_renderer *renderer, char letter,
                        owl_v2 offset, struct owl_glyph *glyph);

OWL_END_DECLARATIONS

#endif

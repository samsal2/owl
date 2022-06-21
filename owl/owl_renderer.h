#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "owl_definitions.h"
#include "owl_texture_2d.h"
#include "owl_texture_cube.h"
#include "owl_vector.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_renderer_packed_char {
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

struct owl_renderer_glyph {
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

#define OWL_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define OWL_IN_FLIGHT_FRAME_COUNT 3
#define OWL_MAX_GARBAGE_COUNT 8
#define OWL_FONT_FIRST_CHAR ((int)(' '))
#define OWL_FONT_CHAR_COUNT ((int)('~' - ' '))

struct owl_plataform;

struct owl_renderer_upload_allocator {
  int32_t in_use;
  uint64_t size;
  void *data;
  VkBuffer buffer;
  VkDeviceMemory memory;
};

struct owl_renderer_upload_allocation {
  VkBuffer buffer;
};

struct owl_renderer_bump_allocator_slot {
  void *data;
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorSet common_descriptor_set;
  VkDescriptorSet model1_descriptor_set;
  VkDescriptorSet model2_descriptor_set;
};

#define OWL_RENDERER_BUMP_ALLOCATOR_SLOT_COUNT 8

struct owl_renderer_bump_allocator {
  int32_t start;
  int32_t end;

  uint64_t size;
  uint64_t offset;
  uint64_t alignment;

  struct owl_renderer_bump_allocator_slot
      slots[OWL_RENDERER_BUMP_ALLOCATOR_SLOT_COUNT];
};

struct owl_renderer_bump_allocation {
  uint32_t offset32;
  uint64_t offset;
  struct owl_renderer_bump_allocator_slot *slot;
};

struct owl_renderer_frame {
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkFence in_flight_fence;
  VkSemaphore acquire_semaphore;
  VkSemaphore render_done_semaphore;
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

  VkCommandBuffer immediate_command_buffer;

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

  VkSampler linear_sampler;

  struct owl_renderer_upload_allocator upload_allocator;

  int32_t skybox_loaded;
  struct owl_texture_cube skybox;

  int32_t font_loaded;
  struct owl_texture_2d font_atlas;
  struct owl_renderer_packed_char font_chars[OWL_FONT_CHAR_COUNT];

  uint32_t frame;
  uint32_t frame_count;

  struct owl_renderer_frame frames[OWL_IN_FLIGHT_FRAME_COUNT];
  struct owl_renderer_bump_allocator allocators[OWL_IN_FLIGHT_FRAME_COUNT];

  PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
  PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
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
owl_renderer_bump_allocate(struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_bump_allocation *allocation);

OWL_PUBLIC void *
owl_renderer_upload_allocate(struct owl_renderer *renderer, uint64_t size,
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
    owl_v2 offset, struct owl_renderer_glyph *glyph);

OWL_END_DECLS

#endif

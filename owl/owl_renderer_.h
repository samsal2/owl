#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "owl_definitions.h"
#include "owl_glyph.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_window;
struct owl_renderer;
struct owl_model;

enum owl_memory_properties {
  OWL_MEMORY_PROPERTIES_CPU_ONLY,
  OWL_MEMORY_PROPERTIES_GPU_ONLY
};

enum owl_renderer_sampler_mip_mode {
  OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST,
  OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR
};

enum owl_renderer_sampler_filter {
  OWL_RENDERER_SAMPLER_FILTER_NEAREST,
  OWL_RENDERER_SAMPLER_FILTER_LINEAR
};

enum owl_renderer_pixel_format {
  OWL_RENDERER_PIXEL_FORMAT_R8_UNORM,
  OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_renderer_sampler_addr_mode {
  OWL_RENDERER_SAMPLER_ADDR_MODE_REPEAT,
  OWL_RENDERER_SAMPLER_ADDR_MODE_MIRRORED_REPEAT,
  OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE,
  OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER
};

enum owl_renderer_image_src_type {
  OWL_RENDERER_IMAGE_SRC_TYPE_FILE,
  OWL_RENDERER_IMAGE_SRC_TYPE_DATA
};

struct owl_renderer_frame_heap_reference {
  owl_u32 offset32;
  owl_u64 offset;
  VkBuffer buffer;
  VkDescriptorSet common_ubo_set;
  VkDescriptorSet model1_ubo_set;
  VkDescriptorSet model2_ubo_set;
};

struct owl_renderer_image_desc {
  enum owl_renderer_image_src_type src_type;

  char const *src_path;

  owl_byte const *src_data;
  owl_i32 src_data_width;
  owl_i32 src_data_height;
  enum owl_renderer_pixel_format src_data_pixel_format;

  owl_b32 sampler_use_default;
  enum owl_renderer_sampler_mip_mode sampler_mip_mode;
  enum owl_renderer_sampler_filter sampler_min_filter;
  enum owl_renderer_sampler_filter sampler_mag_filter;
  enum owl_renderer_sampler_addr_mode sampler_wrap_u;
  enum owl_renderer_sampler_addr_mode sampler_wrap_v;
  enum owl_renderer_sampler_addr_mode sampler_wrap_w;
};

struct owl_renderer_font_desc {
  char const *path;
  owl_i32 size;
};

struct owl_pcu_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_pnuujw_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_v4 joints0;
  owl_v4 weights0;
};

struct owl_renderer_common_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

struct owl_renderer_quad {
  owl_v2 position0;
  owl_v2 position1;
  owl_v3 color;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_renderer_image_id texture;
};

struct owl_renderer_vertex_list {
  owl_renderer_image_id texture;

  owl_i32 index_count;
  owl_u32 const *indices;

  owl_i32 vertex_count;
  struct owl_renderer_vertex const *vertices;
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

  owl_u32 vk_device_option_count;
  VkPhysicalDevice vk_device_options[OWL_VK_CONTEXT_MAX_DEVICE_OPTION_COUNT];
};

enum owl_pipeline_id {
  OWL_PIPELINE_ID_BASIC,
  OWL_PIPELINE_ID_WIRES,
  OWL_PIPELINE_ID_TEXT,
  OWL_PIPELINE_ID_MODEL,
  OWL_PIPELINE_ID_COUNT,
  OWL_PIPELINE_ID_NONE = OWL_PIPELINE_ID_COUNT
};

struct owl_vk_pipeline_manager {
  VkDescriptorSetLayout vk_vert_ubo_set_layout;
  VkDescriptorSetLayout vk_frag_ubo_set_layout;
  VkDescriptorSetLayout vk_both_ubo_set_layout;
  VkDescriptorSetLayout vk_vert_ssbo_set_layout;
  VkDescriptorSetLayout vk_frag_image_set_layout;

  VkShaderModule vk_basic_vert_shader;
  VkShaderModule vk_basic_frag_shader;
  VkShaderModule vk_text_frag_shader;
  VkShaderModule vk_model_vert_shader;
  VkShaderModule vk_model_frag_shader;

  VkPipelineLayout vk_common_pipeline_layout;
  VkPipelineLayout vk_model_pipeline_layout;

  enum owl_pipeline_id active_pipeline;

  VkPipeline vk_pipelines[OWL_PIPELINE_ID_COUNT];
  VkPipelineLayout vk_pipeline_layouts[OWL_PIPELINE_ID_COUNT];
};

enum owl_vk_attachment_type {
  OWL_VK_ATTACHMENT_TYPE_COLOR,
  OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL
};

struct owl_vk_attachment {
  owl_u32 width;
  owl_u32 height;
  VkImage vk_image;
  VkDeviceMemory vk_memory;
  VkImageView vk_image_view;
};

#define OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT 8
struct owl_vk_swapchain {
  VkSwapchainKHR vk_swapchain;

  VkExtent2D size;
  VkClearValue clear_values[2];

  owl_u32 active_image;
  owl_u32 vk_image_count;
  VkImage vk_images[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
  VkImageView vk_image_views[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
  VkFramebuffer vk_framebuffers[OWL_VK_SWAPCHAIN_MAX_IMAGE_COUNT];
};

struct owl_vk_frame_heap {
  void *data;
  VkDeviceSize offset;
  VkDeviceSize size;
  VkDeviceSize alignment;
  VkDeviceMemory vk_memory;
  VkBuffer vk_buffer;
  VkDescriptorSet vk_vert_ubo_set;
  VkDescriptorSet vk_both_ubo_set;
};

#define OWL_VK_FRAME_HEAP_GARBAGE_MAX_HEAP_COUNT 16
struct owl_vk_frame_heap_garbage {
  owl_i32 heap_count;
  struct owl_vk_frame_heap heaps[OWL_VK_FRAME_HEAP_GARBAGE_MAX_HEAP_COUNT];
};

struct owl_vk_frame_sync {
  VkFence vk_in_flight_fence;
  VkSemaphore vk_render_done_semaphore;
  VkSemaphore vk_image_available_semaphore;
};

struct owl_vk_frame {
  VkCommandPool vk_command_pool;
  VkCommandBuffer vk_command_buffer;
  struct owl_vk_frame_sync sync;
  struct owl_vk_frame_heap heap;
};

struct owl_image {
  VkImage vk_image;
  VkDeviceMemory vk_memory;
  VkImageView vk_image_view;
  VkDescriptorSet vk_set;
  VkSampler vk_sampler;
};

#define OWL_VK_IMAGE_POOL_MAX_SLOT_COUNT 8
struct owl_image_pool {
  owl_i32 slots[OWL_VK_IMAGE_POOL_MAX_SLOT_COUNT];
  struct owl_image images[OWL_VK_IMAGE_POOL_MAX_SLOT_COUNT];
};
typedef owl_i32 owl_image_id;

struct owl_font {
  owl_image_id atlas;
};

#define OWL_VK_FONT_POOL_MAX_SLOT_COUNT 8
struct owl_font_pool {
  owl_i32 slots[OWL_VK_FONT_POOL_MAX_SLOT_COUNT];
  struct owl_font fonts[OWL_VK_FONT_POOL_MAX_SLOT_COUNT];
};
typedef owl_i32 owl_font_id;

struct owl_pvm_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 matrix;
};

struct owl_camera {
  owl_m4 projection;
  owl_m4 view;
};

struct owl_renderer_stats {
  double time_stamp_current;
  double time_stamp_previous;
  double time_stamp_delta;

  float framebuffer_ratio;
  owl_i32 framebuffer_width;
  owl_i32 framebuffer_height;
  owl_i32 window_width;
  owl_i32 window_height;
};

struct owl_renderer {
  struct owl_vk_context context;
  struct owl_vk_attachment color_attachment;
  struct owl_vk_attachment depth_attachment;
  struct owl_vk_swapchain swapchain;
  struct owl_vk_pipeline_manager pipeline_manager;

  VkCommandPool command_pool;
  VkDescriptorPool set_pool;

  VkCommandBuffer im_command_buffer;

  struct owl_image_pool image_pool;
  struct owl_font_pool font_pool;

  owl_i32 active_frame;
  struct owl_vk_frame_heap_garbage frame_heap_garbage;
  struct owl_vk_frame frames[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
};

owl_public enum owl_code
owl_renderer_init (struct owl_renderer *r, struct owl_window const *w);

owl_public enum owl_code
owl_renderer_swapchain_resize (struct owl_renderer *r,
                               struct owl_window const *w);

owl_public void
owl_renderer_deinit (struct owl_renderer *r);

owl_public owl_u32
owl_renderer_find_memory_type (struct owl_renderer const *r, owl_u32 filter,
                               enum owl_renderer_memory_visibility vis);

owl_public owl_b32
owl_renderer_frame_heap_offset_is_clear (struct owl_renderer const *r);

owl_public void
owl_renderer_frame_heap_offset_clear (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_image_init (struct owl_renderer *r,
                         struct owl_renderer_image_desc const *desc,
                         owl_renderer_image_id *img);

owl_public void
owl_renderer_image_deinit (struct owl_renderer *r, owl_renderer_image_id img);

owl_public void *
owl_renderer_frame_heap_allocate (
    struct owl_renderer *r, owl_u64 sz,
    struct owl_renderer_frame_heap_reference *ref);

owl_public enum owl_code
owl_renderer_frame_heap_submit (struct owl_renderer *r, owl_u64 sz,
                                void const *src,
                                struct owl_renderer_frame_heap_reference *ref);

owl_public enum owl_code
owl_renderer_pipeline_bind (struct owl_renderer *r,
                            enum owl_renderer_pipeline pipeline);

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_init (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_begin (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_end (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_submit (struct owl_renderer *r);

owl_public void
owl_renderer_immidiate_command_buffer_deinit (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_frame_begin (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_frame_end (struct owl_renderer *r);

owl_public enum owl_code
owl_renderer_font_init (struct owl_renderer *r, char const *path, owl_i32 sz,
                        owl_renderer_font_id *font);

owl_public void
owl_renderer_font_deinit (struct owl_renderer *r, owl_renderer_font_id font);

owl_public void
owl_renderer_active_font_set (struct owl_renderer *r,
                              owl_renderer_font_id font);

owl_public enum owl_code
owl_renderer_font_fill_glyph (struct owl_renderer const *r, char c,
                              owl_v2 offset, owl_renderer_image_id font,
                              struct owl_glyph *glyph);

owl_public enum owl_code
owl_renderer_active_font_fill_glyph (struct owl_renderer const *r, char c,
                                     owl_v2 offset, struct owl_glyph *glyph);

owl_public enum owl_code
owl_renderer_quad_draw (struct owl_renderer *r, owl_m4 const matrix,
                        struct owl_renderer_quad const *quad);

owl_public enum owl_code
owl_renderer_model_draw (struct owl_renderer *r, owl_m4 const matrix,
                         struct owl_model const *model);

owl_public enum owl_code
owl_renderer_text_draw (struct owl_renderer *r, owl_v2 const pos,
                        owl_v3 const color, char const *text);

owl_public enum owl_code
owl_renderer_vertex_list_draw (struct owl_renderer *r, owl_m4 const matrix,
                               struct owl_renderer_vertex_list *list);

OWL_END_DECLS

#endif

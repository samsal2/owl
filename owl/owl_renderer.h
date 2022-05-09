#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "owl_definitions.h"
#include "owl_glyph.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_window;
struct owl_renderer;
struct owl_model;

enum owl_renderer_memory_visibility {
  OWL_RENDERER_MEMORY_VISIBILITY_CPU,
  OWL_RENDERER_MEMORY_VISIBILITY_CPU_CACHED,
  OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT,
  OWL_RENDERER_MEMORY_VISIBILITY_GPU
};

enum owl_renderer_pipeline {
  OWL_RENDERER_PIPELINE_MAIN,
  OWL_RENDERER_PIPELINE_WIRES,
  OWL_RENDERER_PIPELINE_FONT,
  OWL_RENDERER_PIPELINE_MODEL,
  OWL_RENDERER_PIPELINE_COUNT,
  OWL_RENDERER_PIPELINE_NONE = OWL_RENDERER_PIPELINE_COUNT
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

struct owl_renderer_font_packed_char {
  owl_u16 x0;
  owl_u16 y0;
  owl_u16 x1;
  owl_u16 y1;
  float x_offset;
  float y_offset;
  float x_advance;
  float x_offset2;
  float y_offset2;
};

struct owl_renderer_font_glyph {
  owl_v3 positions[4];
  owl_v2 uvs[4];
};

struct owl_renderer_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
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

struct owl_renderer {
  /* ====================================================================== */
  /* time_stamps */
  /* ====================================================================== */
  double time_stamp_current;
  double time_stamp_previous;
  double time_stamp_delta;
  /* ====================================================================== */

  /* ====================================================================== */
  /* world */
  /* ====================================================================== */
  owl_m4 camera_projection;
  owl_m4 camera_view;
  /* ====================================================================== */

  /* ====================================================================== */
  /* dimensions */
  /* ====================================================================== */
  float framebuffer_ratio;
  owl_i32 framebuffer_width;
  owl_i32 framebuffer_height;
  owl_i32 window_width;
  owl_i32 window_height;
  /* ====================================================================== */

  /* ====================================================================== */
  /* instance */
  /* ====================================================================== */
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  /* ====================================================================== */

  /* ====================================================================== */
  /* surface */
  /* ====================================================================== */
  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;
  /* ====================================================================== */

  /* ====================================================================== */
  /* sampling */
  /* ====================================================================== */
  VkSampleCountFlagBits msaa_sample_count;
  /* ====================================================================== */

  /* ====================================================================== */
  /* device */
  /* ====================================================================== */
  VkPhysicalDevice physical_device;
  VkDevice device;

  owl_u32 graphics_queue_family;
  owl_u32 present_queue_family;

  VkQueue graphics_queue;
  VkQueue present_queue;

  owl_u32 device_option_count;
  VkPhysicalDevice device_options[OWL_RENDERER_MAX_DEVICE_OPTION_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* swapchain */
  /* ====================================================================== */
  VkSwapchainKHR swapchain;

  VkExtent2D swapchain_extent;
  VkPresentModeKHR swapchain_present_mode;
  VkClearValue swapchain_clear_values[2];

  owl_u32 active_swapchain_image_index;
  VkImage active_swapchain_image;

  owl_u32 swapchain_image_count;
  VkImage swapchain_images[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkImageView swapchain_image_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose pools */
  /* ====================================================================== */
  VkCommandPool transient_command_pool;
  VkDescriptorPool common_set_pool;
  /* ====================================================================== */

  /* ====================================================================== */
  /* general immidiate usage command buffer */
  /* ====================================================================== */
  VkCommandBuffer immidiate_command_buffer;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main render pass  */
  /* ====================================================================== */
  VkRenderPass main_render_pass;
  /* ====================================================================== */

  /* ====================================================================== */
  /* color attachment */
  /* ====================================================================== */
  VkImage color_image;
  VkImageView color_image_view;
  VkDeviceMemory color_memory;
  /* ====================================================================== */

  /* ====================================================================== */
  /* depth attachment */
  /* ====================================================================== */
  VkFormat depth_stencil_format;
  VkImage depth_stencil_image;
  VkImageView depth_stencil_image_view;
  VkDeviceMemory depth_stencil_memory;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main framebuffers */
  /* ====================================================================== */
  VkFramebuffer active_swapchain_framebuffer;
  VkFramebuffer swapchain_framebuffers[OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* sets layouts */
  /* ====================================================================== */
  VkDescriptorSetLayout vertex_ubo_set_layout;
  VkDescriptorSetLayout fragment_ubo_set_layout;
  VkDescriptorSetLayout shared_ubo_set_layout;
  VkDescriptorSetLayout vertex_ssbo_set_layout;
  VkDescriptorSetLayout image_set_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipeline layouts */
  /* ====================================================================== */
  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout model_pipeline_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* shaders */
  /* ====================================================================== */
  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule font_fragment_shader;
  VkShaderModule model_vertex_shader;
  VkShaderModule model_fragment_shader;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipelines */
  /* ====================================================================== */
  VkPipeline active_pipeline;
  VkPipelineLayout active_pipeline_layout;

  VkPipeline pipelines[OWL_RENDERER_PIPELINE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_RENDERER_PIPELINE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame submition resources */
  /* ====================================================================== */
  owl_i32 active_frame;
  VkCommandBuffer active_frame_command_buffer;
  VkCommandPool active_frame_command_pool;

  VkCommandPool frame_command_pools[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkCommandBuffer frame_command_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame sync primitives */
  /* ====================================================================== */
  VkFence active_in_flight_fence;
  VkSemaphore active_render_done_semaphore;
  VkSemaphore active_image_available_semaphore;

  VkFence in_flight_fences[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore render_done_semaphores[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore image_available_semaphores[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame heap garbage */
  /* ====================================================================== */
  owl_i32 garbage_memory_count;
  VkDeviceMemory garbage_memories[OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT];

  owl_i32 garbage_buffer_count;
  VkBuffer garbage_buffers[OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT];

  owl_i32 garbage_common_set_count;
  VkDescriptorSet garbage_common_sets[OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT];

  owl_i32 garbage_model2_set_count;
  VkDescriptorSet garbage_model1_sets[OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT];

  owl_i32 garbage_model1_set_count;
  VkDescriptorSet garbage_model2_sets[OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame heap resources */
  /* ====================================================================== */
  VkDeviceMemory frame_heap_memory;

  owl_u64 frame_heap_buffer_size;
  owl_u64 frame_heap_buffer_alignment;
  owl_u64 frame_heap_buffer_aligned_size;

  owl_byte *active_frame_heap_data;
  VkBuffer active_frame_heap_buffer;
  VkDescriptorSet active_frame_heap_common_set;
  VkDescriptorSet active_frame_heap_model1_set;
  VkDescriptorSet active_frame_heap_model2_set;

  owl_byte *frame_heap_data[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  owl_u64 frame_heap_offsets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkBuffer frame_heap_buffers[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet frame_heap_common_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet frame_heap_model1_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  VkDescriptorSet frame_heap_model2_sets[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* image pool resources */
  /* ====================================================================== */
  owl_i32 image_pool_slots[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  VkImage image_pool_images[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  VkDeviceMemory image_pool_memories[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  VkImageView image_pool_image_views[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  VkSampler image_pool_samplers[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  VkDescriptorSet image_pool_sets[OWL_RENDERER_IMAGE_POOL_SLOT_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* font pool resources */
  /* ====================================================================== */
  owl_renderer_image_id active_font;

  owl_i32 font_pool_slots[OWL_RENDERER_FONT_POOL_SLOT_COUNT];
  owl_renderer_image_id font_pool_atlases[OWL_RENDERER_FONT_POOL_SLOT_COUNT];
  struct owl_packed_glyph
      font_pool_packed_glyphs[94][OWL_RENDERER_FONT_POOL_SLOT_COUNT];
  /* ====================================================================== */
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

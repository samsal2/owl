#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "owl_internal.h"
#include "owl_types.h"

#include <vulkan/vulkan.h>

struct owl_window;
struct owl_renderer;

#define OWL_RENDERER_MEMORY_TYPE_NONE (owl_u32) - 1
#define OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT 8
#define OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT 2
#define OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT 8
#define OWL_RENDERER_MAX_DEVICE_OPTIONS_COUNT 8
#define OWL_RENDERER_CLEAR_VALUES_COUNT 2
#define OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT 32

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
  OWL_RENDERER_PIPELINE_COUNT
};
#define OWL_RENDERER_PIPELINE_NONE OWL_RENDERER_PIPELINE_COUNT

typedef enum owl_code (*owl_vk_create_surface_fn)(
    struct owl_renderer const *renderer, void const *user_data,
    VkSurfaceKHR *surface);

struct owl_renderer_init_desc {
  char const *name;

  float framebuffer_ratio;
  owl_i32 framebuffer_width;
  owl_i32 framebuffer_height;
  owl_i32 window_width;
  owl_i32 window_height;

  owl_i32 instance_extensions_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  owl_vk_create_surface_fn create_surface;
};

struct owl_renderer_dynamic_heap_reference {
  owl_u32 offset32;
  owl_u64 offset;
  VkBuffer buffer;
  VkDescriptorSet common_ubo_set;
  VkDescriptorSet model_ubo_set;
  VkDescriptorSet model_ubo_params_set;
};

struct owl_renderer_image {
  owl_i32 slot;
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

struct owl_renderer_image_init_desc {
  enum owl_renderer_image_src_type src_type;

  char const *src_path;

  owl_byte const *src_data;
  owl_i32 src_data_width;
  owl_i32 src_data_height;
  enum owl_renderer_pixel_format src_data_pixel_format;

  owl_b32 use_default_sampler;
  enum owl_renderer_sampler_mip_mode sampler_mip_mode;
  enum owl_renderer_sampler_filter sampler_min_filter;
  enum owl_renderer_sampler_filter sampler_mag_filter;
  enum owl_renderer_sampler_addr_mode sampler_wrap_u;
  enum owl_renderer_sampler_addr_mode sampler_wrap_v;
  enum owl_renderer_sampler_addr_mode sampler_wrap_w;
};

struct owl_renderer {
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
  VkSampleCountFlags msaa_sample_count;
  /* ====================================================================== */

  /* ====================================================================== */
  /* device */
  /* ====================================================================== */
  VkPhysicalDevice physical_device;
  VkDevice device;

  owl_u32 graphics_queue_family_index;
  owl_u32 present_queue_family_index;

  VkQueue graphics_queue;
  VkQueue present_queue;

  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceMemoryProperties device_memory_properties;

  owl_u32 device_options_count;
  VkPhysicalDevice device_options[OWL_RENDERER_MAX_DEVICE_OPTIONS_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* swapchain */
  /* ====================================================================== */
  VkSwapchainKHR swapchain;

  VkExtent2D swapchain_extent;
  VkPresentModeKHR swapchain_present_mode;
  VkClearValue swapchain_clear_values[OWL_RENDERER_CLEAR_VALUES_COUNT];

  owl_u32 active_swapchain_image_index;
  VkImage active_swapchain_image;

  owl_u32 swapchain_images_count;
  VkImage swapchain_images[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT];
  VkImageView swapchain_image_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT];
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
  VkFramebuffer swapchain_framebuffers[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT];
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

  VkCommandPool frame_command_pools[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkCommandBuffer frame_command_buffers[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame sync primitives */
  /* ====================================================================== */
  VkFence active_in_flight_fence;
  VkSemaphore active_render_done_semaphore;
  VkSemaphore active_image_available_semaphore;

  VkFence in_flight_fences[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkSemaphore render_done_semaphores[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkSemaphore image_available_semaphores[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* dynamic heap garbage */
  /* ====================================================================== */
  owl_i32 garbage_memories_count;
  VkDeviceMemory garbage_memories[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  owl_i32 garbage_buffers_count;
  VkBuffer garbage_buffers[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  owl_i32 garbage_common_ubo_sets_count;
  VkDescriptorSet garbage_common_ubo_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  owl_i32 garbage_model_ubo_sets_count;
  VkDescriptorSet garbage_model_ubo_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  owl_i32 garbage_model_ubo_params_sets_count;
  VkDescriptorSet
      garbage_model_ubo_params_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* dynamic heap resources */
  /* ====================================================================== */
  VkDeviceMemory dynamic_heap_memory;

  owl_u64 dynamic_heap_offset;
  owl_u64 dynamic_heap_buffer_size;
  owl_u64 dynamic_heap_buffer_alignment;
  owl_u64 dynamic_heap_buffer_aligned_size;

  owl_byte *active_dynamic_heap_data;
  VkBuffer active_dynamic_heap_buffer;
  VkDescriptorSet active_dynamic_heap_common_ubo_set;
  VkDescriptorSet active_dynamic_heap_model_ubo_set;
  VkDescriptorSet active_dynamic_heap_model_ubo_params_set;

  owl_byte *dynamic_heap_data[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkBuffer dynamic_heap_buffers[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet
      dynamic_heap_common_ubo_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet
      dynamic_heap_model_ubo_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet
      dynamic_heap_model_ubo_params_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* image manager resources */
  /* ====================================================================== */
  owl_i32 image_manager_slots[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];

  VkImage image_manager_images[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];
  VkDeviceMemory image_manager_memories[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];
  VkImageView image_manager_image_views[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];

  /* TODO(samuel): image manager sampler cache */
  VkSampler image_manager_samplers[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];
  VkDescriptorSet image_manager_sets[OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT];
  /* ====================================================================== */
};

enum owl_code owl_renderer_init(struct owl_renderer_init_desc const *desc,
                                struct owl_renderer *r);
enum owl_code
owl_renderer_swapchain_resize(struct owl_renderer_init_desc const *desc,
                              struct owl_renderer *r);
void owl_renderer_deinit(struct owl_renderer *r);
owl_u32 owl_renderer_find_memory_type(struct owl_renderer const *r,
                                      VkMemoryRequirements const *req,
                                      enum owl_renderer_memory_visibility vis);
owl_b32 owl_renderer_dynamic_heap_is_clear(struct owl_renderer const *r);
void owl_renderer_dynamic_heap_clear(struct owl_renderer *r);
enum owl_code
owl_renderer_image_init(struct owl_renderer *r,
                        struct owl_renderer_image_init_desc const *desc,
                        struct owl_renderer_image *img);
void owl_renderer_image_deinit(struct owl_renderer *r,
                               struct owl_renderer_image *img);
void *owl_renderer_dynamic_heap_alloc(
    struct owl_renderer *r, owl_u64 size,
    struct owl_renderer_dynamic_heap_reference *ref);
enum owl_code owl_renderer_dynamic_heap_submit(
    struct owl_renderer *r, owl_u64 size, void const *src,
    struct owl_renderer_dynamic_heap_reference *ref);
enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *r,
                                         enum owl_renderer_pipeline type);
enum owl_code
owl_renderer_immidiate_command_buffer_init(struct owl_renderer *r);
enum owl_code
owl_renderer_immidiate_command_buffer_begin(struct owl_renderer *r);
enum owl_code owl_renderer_immidiate_command_buffer_end(struct owl_renderer *r);
enum owl_code
owl_renderer_immidiate_command_buffer_submit(struct owl_renderer *r);
void owl_renderer_immidiate_command_buffer_deinit(struct owl_renderer *r);
enum owl_code owl_renderer_begin_frame(struct owl_renderer *r);
enum owl_code owl_renderer_end_frame(struct owl_renderer *r);

#endif

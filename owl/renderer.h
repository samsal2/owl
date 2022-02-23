#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_window;
struct owl_renderer;

#define OWL_MEMORY_TYPE_NONE (owl_u32) - 1
#define OWL_RENDERER_MAX_SWAPCHAIN_IMAGES 8
#define OWL_RENDERER_DYNAMIC_BUFFER_COUNT 2
#define OWL_RENDERER_MAX_GARBAGE_ITEMS 8
#define OWL_RENDERER_MAX_DEVICE_OPTIONS 8
#define OWL_RENDERER_CLEAR_VALUES_COUNT 2
#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT
#define OWL_RENDERER_MAX_TEXTURE_COUNT 64

enum owl_memory_visibility {
  OWL_MEMORY_VISIBILITY_CPU_ONLY,
  OWL_MEMORY_VISIBILITY_GPU_ONLY,
  OWL_MEMORY_VISIBILITY_CPU_TO_GPU
};

enum owl_pipeline_type {
  OWL_PIPELINE_TYPE_MAIN,
  OWL_PIPELINE_TYPE_WIRES,
  OWL_PIPELINE_TYPE_FONT,
  OWL_PIPELINE_TYPE_SKYBOX,
  OWL_PIPELINE_TYPE_PBR,
  OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND,
  OWL_PIPELINE_TYPE_COUNT
};

struct owl_renderer_init_info {
  char const *name;

  int framebuffer_width;
  int framebuffer_height;

  int instance_extensions_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  enum owl_code (*create_surface)(struct owl_renderer const *r,
                                  void const *user_data, VkSurfaceKHR *surface);
};

struct owl_dynamic_buffer_reference {
  owl_u32 offset32;
  VkDeviceSize offset;
  VkBuffer buffer;
  VkDescriptorSet pvm_set;
};

struct owl_renderer {
  /* ====================================================================== */
  /* dims */
  /* ====================================================================== */
  int width;
  int height;
  /* ====================================================================== */

  /* ====================================================================== */
  /* instance */
  /* ====================================================================== */
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug;
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

  owl_u32 graphics_family_index;
  VkQueue graphics_queue;

  owl_u32 present_family_index;
  VkQueue present_queue;

  owl_u32 device_options_count;
  VkPhysicalDevice device_options[OWL_RENDERER_MAX_DEVICE_OPTIONS];

  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceMemoryProperties device_memory_properties;
  /* ====================================================================== */

  /* ====================================================================== */
  /* swapchain */
  /* ====================================================================== */
  VkSwapchainKHR swapchain;

  VkExtent2D swapchain_extent;
  VkPresentModeKHR swapchain_present_mode;
  VkClearValue swapchain_clear_values[OWL_RENDERER_CLEAR_VALUES_COUNT];

  owl_u32 swapchain_active_image;
  owl_u32 swapchain_images_count;
  VkImage swapchain_images[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose pools */
  /* ====================================================================== */
  VkCommandPool transient_command_pool;
  VkDescriptorPool common_set_pool;
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
  VkImageView color_view;
  VkDeviceMemory color_memory;
  /* ====================================================================== */

  /* ====================================================================== */
  /* depth attachment */
  /* ====================================================================== */
  VkImage depth_stencil_image;
  VkImageView depth_stencil_view;
  VkDeviceMemory depth_stencil_memory;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main framebuffers */
  /* ====================================================================== */
  VkFramebuffer swapchain_framebuffers[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* sets layouts */
  /* ====================================================================== */
  VkDescriptorSetLayout pvm_set_layout;
  VkDescriptorSetLayout texture_set_layout;
  VkDescriptorSetLayout scene_set_layout;
  VkDescriptorSetLayout material_set_layout;
  VkDescriptorSetLayout node_set_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main pipeline layouts */
  /* ====================================================================== */
  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout pbr_pipeline_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* shaders */
  /* ====================================================================== */
  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule font_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;
  VkShaderModule pbr_vertex_shader;
  VkShaderModule pbr_fragment_shader;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipelines */
  /* ====================================================================== */
  enum owl_pipeline_type bound_pipeline;
  VkPipeline pipelines[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_PIPELINE_TYPE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame submition resources */
  /* ====================================================================== */
  int active_frame;
  VkCommandBuffer active_command_buffer;
  VkCommandPool frame_command_pools[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkCommandBuffer frame_command_buffers[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame sync primitives */
  /* ====================================================================== */
  VkSemaphore image_available_semaphores[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkSemaphore render_done_semaphores[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkFence in_flight_fences[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering garbage */
  /* ====================================================================== */
  int garbage_memories_count;
  VkDeviceMemory garbage_memories[OWL_RENDERER_MAX_GARBAGE_ITEMS];

  int garbage_buffers_count;
  VkBuffer garbage_buffers[OWL_RENDERER_MAX_GARBAGE_ITEMS];

  int garbage_pvm_sets_count;
  VkDescriptorSet garbage_pvm_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering resources */
  /* ====================================================================== */
  VkDeviceMemory dynamic_memory;

  VkDeviceSize dynamic_buffer_size;
  VkDeviceSize dynamic_buffer_alignment;
  VkDeviceSize dynamic_buffer_aligned_size;

  owl_byte *dynamic_data[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkBuffer dynamic_buffers[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkDeviceSize dynamic_offsets[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkDescriptorSet dynamic_pvm_sets[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* irradiance cube texture */
  /* ====================================================================== */
  VkImage irradiance_cube_image;
  VkDeviceMemory irradiance_cube_memory;
  VkImageView irradiance_cube_view;
  VkSampler irradiance_cube_sampler;
  /* ====================================================================== */

  /* ====================================================================== */
  /* prefiltered cube texture */
  /* ====================================================================== */
  VkImage prefiltered_cube_image;
  VkDeviceMemory prefiltered_cube_memory;
  VkImageView prefiltered_cube_view;
  VkSampler prefiltered_cube_sampler;
  /* ====================================================================== */

  /* ====================================================================== */
  /*  lutbrdf texture */
  /* ====================================================================== */
  VkImage lutbrdf_image;
  VkDeviceMemory lutbrdf_memory;
  VkImageView lutbrdf_view;
  VkSampler lutbrdf_sampler;
  /* ====================================================================== */
};

enum owl_code owl_renderer_init(struct owl_renderer_init_info const *info,
                                struct owl_renderer *r);

enum owl_code
owl_renderer_resize_swapchain(struct owl_renderer_init_info const *info,
                              struct owl_renderer *r);

void owl_renderer_deinit(struct owl_renderer *r);

int owl_renderer_is_dynamic_buffer_clear(struct owl_renderer *r);

void owl_renderer_clear_dynamic_offset(struct owl_renderer *r);

owl_u32 owl_renderer_find_memory_type(struct owl_renderer const *r,
                                      owl_u32 filter,
                                      enum owl_memory_visibility vis);

void *
owl_renderer_dynamic_buffer_alloc(struct owl_renderer *r, VkDeviceSize size,
                                  struct owl_dynamic_buffer_reference *ref);

enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *r,
                                         enum owl_pipeline_type type);

enum owl_code
owl_renderer_alloc_single_use_command_buffer(struct owl_renderer const *r,
                                             VkCommandBuffer *command);

enum owl_code
owl_renderer_free_single_use_command_buffer(struct owl_renderer const *r,
                                            VkCommandBuffer command);

void owl_renderer_clear_dynamic_offset(struct owl_renderer *r);

enum owl_code owl_renderer_begin_frame(struct owl_renderer *r);

enum owl_code owl_renderer_end_frame(struct owl_renderer *r);
#endif

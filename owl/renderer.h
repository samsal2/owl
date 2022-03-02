#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_window;
struct owl_renderer;

#define OWL_MEMORY_TYPE_NONE (owl_u32) - 1
#define OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT 8
#define OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT 2
#define OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT 8
#define OWL_RENDERER_MAX_DEVICE_OPTIONS_COUNT 8
#define OWL_RENDERER_CLEAR_VALUES_COUNT 2
#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

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
  OWL_PIPELINE_TYPE_SCENE,
  OWL_PIPELINE_TYPE_COUNT
};

typedef enum owl_code (*owl_vk_surface_init_callback)(
    struct owl_renderer const *r, void const *user_data, VkSurfaceKHR *surface);

struct owl_renderer_init_info {
  char const *name;

  int framebuffer_width;
  int framebuffer_height;

  int instance_extensions_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  owl_vk_surface_init_callback create_surface;
};

struct owl_dynamic_heap_reference {
  owl_u32 offset32;
  VkDeviceSize offset;
  VkBuffer buffer;
  VkDescriptorSet pvm_set;
  VkDescriptorSet scene_set;
};

struct owl_static_heap_reference {
  int slot;
  VkDeviceSize offset;
  VkDeviceMemory memory;
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

  owl_u32 graphics_family_index;
  VkQueue graphics_queue;

  owl_u32 present_family_index;
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
  VkImageView swapchain_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT];
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
  VkFormat depth_stencil_format;
  VkImage depth_stencil_image;
  VkImageView depth_stencil_view;
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
  VkDescriptorSetLayout pvm_set_layout;
  VkDescriptorSetLayout texture_set_layout;
  VkDescriptorSetLayout scene_set_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipeline layouts */
  /* ====================================================================== */
  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout scene_pipeline_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* shaders */
  /* ====================================================================== */
  VkShaderModule basic_vertex_shader;
  VkShaderModule basic_fragment_shader;
  VkShaderModule font_fragment_shader;
  VkShaderModule skybox_vertex_shader;
  VkShaderModule skybox_fragment_shader;
  VkShaderModule scene_vertex_shader;
  VkShaderModule scene_fragment_shader;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipelines */
  /* ====================================================================== */
  VkPipeline active_pipeline;
  VkPipelineLayout active_pipeline_layout;

  VkPipeline pipelines[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_PIPELINE_TYPE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame submition resources */
  /* ====================================================================== */
  int frame;
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
  int garbage_memories_count;
  VkDeviceMemory garbage_memories[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  int garbage_buffers_count;
  VkBuffer garbage_buffers[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  int garbage_pvm_sets_count;
  VkDescriptorSet garbage_pvm_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];

  int garbage_scene_sets_count;
  VkDescriptorSet garbage_scene_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* dynamic heap resources */
  /* ====================================================================== */
  VkDeviceMemory dynamic_heap_memory;

  VkDeviceSize dynamic_heap_offset;
  VkDeviceSize dynamic_heap_buffer_size;
  VkDeviceSize dynamic_heap_buffer_alignment;
  VkDeviceSize dynamic_heap_buffer_aligned_size;

  owl_byte *active_dynamic_heap_data;
  VkBuffer active_dynamic_heap_buffer;
  VkDescriptorSet active_dynamic_heap_pvm_set;
  VkDescriptorSet active_dynamic_heap_scene_set;

  owl_byte *dynamic_heap_datas[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkBuffer dynamic_heap_buffers[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet dynamic_heap_pvm_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  VkDescriptorSet dynamic_heap_scene_sets[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];
  /* ====================================================================== */
};

enum owl_code owl_renderer_init(struct owl_renderer_init_info const *rii,
                                struct owl_renderer *r);

enum owl_code
owl_renderer_resize_swapchain(struct owl_renderer_init_info const *rii,
                              struct owl_renderer *r);

void owl_renderer_deinit(struct owl_renderer *r);

owl_u32 owl_renderer_find_memory_type(struct owl_renderer const *r,
                                      VkMemoryRequirements const *requirements,
                                      enum owl_memory_visibility vis);

int owl_renderer_is_dynamic_heap_offset_clear(struct owl_renderer const *r);

void owl_renderer_clear_dynamic_heap_offset(struct owl_renderer *r);

void *owl_renderer_dynamic_heap_alloc(struct owl_renderer *r, VkDeviceSize size,
                                      struct owl_dynamic_heap_reference *dhr);

enum owl_code
owl_renderer_static_heap_alloc(struct owl_renderer *r, VkDeviceSize size,
                               VkDeviceSize alignment, owl_u32 type,
                               struct owl_static_heap_reference *shr);

void owl_renderer_static_heap_free(struct owl_renderer *r,
                                   struct owl_static_heap_reference *shr);

enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *r,
                                         enum owl_pipeline_type type);

struct owl_single_use_command_buffer {
  VkCommandBuffer command_buffer;
};

enum owl_code owl_renderer_init_single_use_command_buffer(
    struct owl_renderer const *r,
    struct owl_single_use_command_buffer *command);

enum owl_code owl_renderer_deinit_single_use_command_buffer(
    struct owl_renderer const *r,
    struct owl_single_use_command_buffer *command);

void owl_renderer_clear_dynamic_heap_offset(struct owl_renderer *r);

enum owl_code owl_renderer_begin_frame(struct owl_renderer *r);

enum owl_code owl_renderer_end_frame(struct owl_renderer *r);

#endif

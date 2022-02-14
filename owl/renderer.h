#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "types.h"

#include <vulkan/vulkan.h>

struct owl_window;
struct owl_vk_renderer;

#define OWL_VK_MEMORY_TYPE_NONE (owl_u32) - 1
#define OWL_VK_RENDERER_MAX_SWAPCHAIN_IMAGES 8
#define OWL_VK_RENDERER_DYN_BUFFER_COUNT 2
#define OWL_VK_RENDERER_MAX_DYN_GARBAGE_ITEMS 8
#define OWL_VK_RENDERER_MAX_DEVICE_OPTIONS 8
#define OWL_VK_RENDERER_CLEAR_VALUES_COUNT 2
#define OWL_VK_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

enum owl_vk_memory_visibility {
  OWL_VK_MEMORY_VISIBILITY_CPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_GPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_CPU_TO_GPU
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

typedef enum owl_code (*owl_vk_surface_callback)(struct owl_vk_renderer const *,
                                                 void const *, VkSurfaceKHR *);

struct owl_vk_renderer_info {
  char const *name;

  int framebuffer_width;
  int framebuffer_height;

  int instance_extension_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  owl_vk_surface_callback create_surface;
};

struct owl_dyn_buffer_ref {
  owl_u32 offset32;
  VkDeviceSize offset;
  VkBuffer buffer;
  VkDescriptorSet pvm_set;
};

struct owl_vk_renderer {
  /* ====================================================================== */
  /* active buffer */
  /* ====================================================================== */
  int active;
  /* ====================================================================== */

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
  VkSampleCountFlags sample_count;
  /* ====================================================================== */

  /* ====================================================================== */
  /* device */
  /* ====================================================================== */
  VkDevice device;
  VkPhysicalDevice physical_device;

  owl_u32 graphics_family_index;
  VkQueue graphics_queue;

  owl_u32 present_family_index;
  VkQueue present_queue;

  owl_u32 device_options_count;
  VkPhysicalDevice device_options[OWL_VK_RENDERER_MAX_DEVICE_OPTIONS];

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
  VkClearValue swapchain_clear_values[OWL_VK_RENDERER_CLEAR_VALUES_COUNT];

  owl_u32 swapchain_active_image;
  owl_u32 swapchain_images_count;
  VkImage swapchain_images[OWL_VK_RENDERER_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[OWL_VK_RENDERER_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose pools */
  /* ====================================================================== */
  VkCommandPool transient_cmd_pool;
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
  VkFramebuffer framebuffers[OWL_VK_RENDERER_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* sets layouts */
  /* ====================================================================== */
  /* common */
  VkDescriptorSetLayout pvm_set_layout;
  VkDescriptorSetLayout texture_set_layout;

  /* skybox */
  VkDescriptorSetLayout skybox_set_layout;

  /* pbr */
  VkDescriptorSetLayout scene_set_layout;
  VkDescriptorSetLayout material_set_layout;
  VkDescriptorSetLayout node_set_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main pipeline layouts */
  /* ====================================================================== */
  VkPipelineLayout common_pipeline_layout;
  VkPipelineLayout skybox_pipeline_layout;
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
  VkCommandPool frame_cmd_pools[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkCommandBuffer frame_cmd_buffers[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame sync primitives */
  /* ====================================================================== */
  VkSemaphore image_available_semaphores[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkSemaphore render_done_semaphores[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkFence in_flight_fences[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering resources */
  /* ====================================================================== */
  VkDeviceMemory dyn_memory;

  VkDeviceSize dyn_buffer_size;
  VkDeviceSize dyn_buffer_aligned_size;
  VkDeviceSize dyn_buffer_alignment;

  owl_byte *dyn_data[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkBuffer dyn_buffers[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkDescriptorSet dyn_pvm_sets[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  VkDeviceSize dyn_offsets[OWL_VK_RENDERER_DYN_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering garbage */
  /* ====================================================================== */
  int dyn_garbage_memories_count;
  VkDeviceMemory dyn_garbage_memories[OWL_VK_RENDERER_MAX_DYN_GARBAGE_ITEMS];

  int dyn_garbage_buffers_count;
  VkBuffer dyn_garbage_buffers[OWL_VK_RENDERER_MAX_DYN_GARBAGE_ITEMS];

  int dyn_garbage_pvm_sets_count;
  VkDescriptorSet dyn_garbage_pvm_sets[OWL_VK_RENDERER_MAX_DYN_GARBAGE_ITEMS];
  /* ====================================================================== */
};

enum owl_code owl_renderer_init(struct owl_vk_renderer_info const *info,
                                struct owl_vk_renderer *r);

enum owl_code owl_renderer_create(struct owl_window *w,
                                  struct owl_vk_renderer **r);

enum owl_code
owl_renderer_reinit_swapchain(struct owl_vk_renderer_info const *info,
                              struct owl_vk_renderer *r);

enum owl_code owl_renderer_recreate_swapchain(struct owl_window *w,
                                              struct owl_vk_renderer *r);

void owl_renderer_deinit(struct owl_vk_renderer *r);

void owl_renderer_destroy(struct owl_vk_renderer *r);

enum owl_code owl_renderer_reserve_dyn_memory(struct owl_vk_renderer *r,
                                              VkDeviceSize size);

int owl_renderer_is_dyn_buffer_clear(struct owl_vk_renderer *r);

void owl_renderer_clear_dyn_offset(struct owl_vk_renderer *r);

void owl_renderer_clear_dyn_garbage(struct owl_vk_renderer *r);

owl_u32 owl_renderer_find_memory_type(struct owl_vk_renderer const *r,
                                      owl_u32 filter,
                                      enum owl_vk_memory_visibility vis);

void *owl_renderer_dyn_alloc(struct owl_vk_renderer *r, VkDeviceSize size,
                             struct owl_dyn_buffer_ref *ref);

enum owl_code owl_renderer_bind_pipeline(struct owl_vk_renderer *r,
                                         enum owl_pipeline_type type);

enum owl_code
owl_renderer_alloc_single_use_cmd_buffer(struct owl_vk_renderer const *r,
                                         VkCommandBuffer *cmd);

enum owl_code
owl_renderer_free_single_use_cmd_buffer(struct owl_vk_renderer const *r,
                                        VkCommandBuffer cmd);

#endif

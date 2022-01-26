#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include "types.h"

#include <vulkan/vulkan.h>

#define OWL_VK_MEMORY_TYPE_NONE (owl_u32) - 1
#define OWL_RENDERER_MAX_SWAPCHAIN_IMAGES 8
#define OWL_RENDERER_DYNAMIC_BUFFER_COUNT 2
#define OWL_RENDERER_MAX_GARBAGE_ITEMS 8
#define OWL_RENDERER_MAX_DEVICE_OPTIONS 8
#define OWL_RENDERER_CLEAR_VALUE_COUNT 2
#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

enum owl_vk_mem_vis {
  OWL_VK_MEMORY_VIS_CPU_ONLY, /**/
  OWL_VK_MEMORY_VIS_GPU_ONLY,
  OWL_VK_MEMORY_VIS_CPU_TO_GPU
};

enum owl_pipeline_type {
  OWL_PIPELINE_TYPE_MAIN,
  OWL_PIPELINE_TYPE_WIRES,
  OWL_PIPELINE_TYPE_FONT,
  OWL_PIPELINE_TYPE_PBR,
  OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND,
  OWL_PIPELINE_TYPE_COUNT
};

struct owl_window;
struct owl_vk_renderer;

typedef enum owl_code (*owl_vk_surface_creator)(struct owl_vk_renderer const *,
                                                void const *, VkSurfaceKHR *);

struct owl_vk_renderer_desc {
  char const *name;

  int framebuffer_width;
  int framebuffer_height;

  int instance_extension_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  owl_vk_surface_creator create_surface;
};

struct owl_dyn_buf_ref {
  owl_u32 offset32;
  VkDeviceSize offset;
  VkBuffer buf;
  VkDescriptorSet pvm_set;
};

struct owl_vk_renderer {
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

  owl_u32 graphics_family;
  VkQueue graphics_queue;

  owl_u32 present_family;
  VkQueue present_queue;

  owl_u32 device_options_count;
  VkPhysicalDevice device_options[OWL_RENDERER_MAX_DEVICE_OPTIONS];

  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceMemoryProperties device_mem_properties;
  /* ====================================================================== */

  /* ====================================================================== */
  /* swapchain */
  /* ====================================================================== */
  VkSwapchainKHR swapchain;
  VkExtent2D swapchain_extent;
  VkPresentModeKHR swapchain_present_mode;
  VkClearValue swapchain_clear_values[OWL_RENDERER_CLEAR_VALUE_COUNT];
  owl_u32 swapchain_active_image;
  owl_u32 swapchain_images_count;
  VkImage swapchain_images[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose pools */
  /* ====================================================================== */
  VkCommandPool transient_cmd_pool;
  VkDescriptorPool common_set_pool;
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame submition resources */
  /* ====================================================================== */
  VkCommandPool frame_cmd_pools[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkCommandBuffer frame_cmd_bufs[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame syncronization primitives */
  /* ====================================================================== */
  VkSemaphore frame_image_available[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkSemaphore frame_render_done[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkFence frame_in_flight[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
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
  VkDeviceMemory color_mem;
  /* ====================================================================== */

  /* ====================================================================== */
  /* depth attachment */
  /* ====================================================================== */
  VkImage depth_image;
  VkImageView depth_view;
  VkDeviceMemory depth_mem;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main framebuffers */
  /* ====================================================================== */
  VkFramebuffer framebuffers[OWL_RENDERER_MAX_SWAPCHAIN_IMAGES];
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
  VkShaderModule basic_vert_shader;
  VkShaderModule basic_frag_shader;
  VkShaderModule font_frag_shader;
  VkShaderModule pbr_vert_shader;
  VkShaderModule pbr_frag_shader;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipelines */
  /* ====================================================================== */
  enum owl_pipeline_type bound_pipeline;
  VkPipeline pipelines[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_PIPELINE_TYPE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering resources */
  /* ====================================================================== */
  int dyn_active_buf;
  VkDeviceMemory dyn_mem;

  VkDeviceSize dyn_buf_size;
  VkDeviceSize dyn_aligned_size;
  VkDeviceSize dyn_alignment;

  owl_byte *dyn_data[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkBuffer dyn_bufs[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkDescriptorSet dyn_pvm_sets[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  VkDeviceSize dyn_offsets[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering garbage */
  /* ====================================================================== */
  int dyn_garbage_mems_count;
  VkDeviceMemory dyn_garbage_mems[OWL_RENDERER_MAX_GARBAGE_ITEMS];

  int dyn_garbage_bufs_count;
  VkBuffer dyn_garbage_bufs[OWL_RENDERER_MAX_GARBAGE_ITEMS];

  int dyn_garbage_pvm_sets_count;
  VkDescriptorSet dyn_garbage_pvm_sets[OWL_RENDERER_MAX_GARBAGE_ITEMS];
  /* ====================================================================== */
};

enum owl_code owl_create_renderer(struct owl_window *w,
                                  struct owl_vk_renderer **r);

enum owl_code owl_recreate_swapchain(struct owl_window *w,
                                     struct owl_vk_renderer *r);

void owl_destroy_renderer(struct owl_vk_renderer *r);

enum owl_code owl_init_vk_renderer(struct owl_vk_renderer_desc const *desc,
                                   struct owl_vk_renderer *r);

void owl_deinit_vk_renderer(struct owl_vk_renderer *r);

enum owl_code owl_reinit_vk_swapchain(struct owl_vk_renderer_desc const *desc,
                                      struct owl_vk_renderer *r);

enum owl_code owl_reserve_dyn_buf_mem(struct owl_vk_renderer *r,
                                      VkDeviceSize size);

int owl_is_dyn_buf_flushed(struct owl_vk_renderer *r);

void owl_flush_dyn_buf(struct owl_vk_renderer *r);

void owl_clear_dyn_buf_garbage(struct owl_vk_renderer *r);

owl_u32 owl_find_vk_mem_type(struct owl_vk_renderer const *r, owl_u32 filter,
                             enum owl_vk_mem_vis vis);

void *owl_dyn_buf_alloc(struct owl_vk_renderer *r, VkDeviceSize size,
                        struct owl_dyn_buf_ref *ref);

enum owl_code owl_bind_pipeline(struct owl_vk_renderer *r,
                                enum owl_pipeline_type type);

#endif

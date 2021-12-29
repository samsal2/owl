#ifndef OWL_VK_RENDERER_H_
#define OWL_VK_RENDERER_H_

#include <owl/pipelines.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_DYN_BUF_COUNT 2
#define OWL_MAX_GARBAGE_ITEMS 8
#define OWL_MAX_DEVICE_OPTIONS 8
#define OWL_CLEAR_VAL_COUNT 2

typedef OwlU32 OwlVkMemoryType;
typedef OwlU32 OwlVkMemoryFilter;
typedef OwlU32 OwlVkQueueFamily;
typedef VkDeviceSize OwlVkDeviceSize;

struct owl_vk_renderer {
  /* ====================================================================== */
  /* dims */
  /* ====================================================================== */
  OwlU32 width;
  OwlU32 height;
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
  /* device */
  /* ====================================================================== */
  VkDevice device;
  VkPhysicalDevice physical_device;

  VkQueue graphics_queue;
  OwlVkQueueFamily graphics_family;

  VkQueue present_queue;
  OwlVkQueueFamily present_family;

  OwlU32 device_options_count;
  VkPhysicalDevice device_options[OWL_MAX_DEVICE_OPTIONS];

  VkPhysicalDeviceFeatures device_features;
  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceMemoryProperties device_mem_properties;

  VkSampleCountFlags samples;
  /* ====================================================================== */

  /* ====================================================================== */
  /* swapchain */
  /* ====================================================================== */
  VkSwapchainKHR swapchain;
  VkExtent2D swapchain_extent;
  VkPresentModeKHR swapchain_present_mode;
  VkClearValue swapchain_clear_vals[OWL_CLEAR_VAL_COUNT];
  OwlU32 swapchain_active_img;
  OwlU32 swapchain_img_count;
  VkImage swapchain_imgs[OWL_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[OWL_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose pools */
  /* ====================================================================== */
  VkCommandPool transient_cmd_pool;
  VkDescriptorPool set_pool;
  /* ====================================================================== */

  /* ====================================================================== */
  /* general purpose samplers */
  /* ====================================================================== */
  VkSampler samplers[OWL_SAMPLER_TYPE_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame submition resources */
  /* ====================================================================== */
  VkCommandPool frame_cmd_pools[OWL_DYN_BUF_COUNT];
  VkCommandBuffer frame_cmd_bufs[OWL_DYN_BUF_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* frame syncronization primitives */
  /* ====================================================================== */
  VkSemaphore frame_img_available[OWL_DYN_BUF_COUNT];
  VkSemaphore frame_render_done[OWL_DYN_BUF_COUNT];
  VkFence frame_in_flight[OWL_DYN_BUF_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* main render pass resources
  /* ====================================================================== */
  VkRenderPass main_render_pass;

  VkImage color_img;
  VkImageView color_view;
  VkDeviceMemory color_mem;

  VkImage depth_img;
  VkImageView depth_view;
  VkDeviceMemory depth_mem;

  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];

  VkDescriptorSetLayout pvm_set_layout;
  VkDescriptorSetLayout tex_set_layout;

  VkPipelineLayout main_pipeline_layout;

  VkShaderModule basic_vert_shader;
  VkShaderModule basic_frag_shader;
  VkShaderModule font_frag_shader;

  enum owl_pipeline_type bound_pipeline;

  VkPipeline pipelines[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_PIPELINE_TYPE_COUNT]; /* non owning */
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering resources */
  /* ====================================================================== */
  OwlU32 dyn_active_buf;
  VkDeviceMemory dyn_mem;

  VkDeviceSize dyn_buf_size;
  VkDeviceSize dyn_aligned_size;
  VkDeviceSize dyn_alignment;

  OwlByte *dyn_data[OWL_DYN_BUF_COUNT];
  VkBuffer dyn_bufs[OWL_DYN_BUF_COUNT];
  VkDescriptorSet dyn_pvm_sets[OWL_DYN_BUF_COUNT];
  VkDeviceSize dyn_offsets[OWL_DYN_BUF_COUNT];
  /* ====================================================================== */

  /* ====================================================================== */
  /* double buffering garbage */
  /* ====================================================================== */
  OwlU32 dyn_garbage_mem_count;
  VkDeviceMemory dyn_garbage_mems[OWL_MAX_GARBAGE_ITEMS];

  OwlU32 dyn_garbage_buf_count;
  VkBuffer dyn_garbage_bufs[OWL_MAX_GARBAGE_ITEMS];

  OwlU32 dyn_garbage_pvm_set_count;
  VkDescriptorSet dyn_garbage_pvm_sets[OWL_MAX_GARBAGE_ITEMS];
  /* ====================================================================== */

  /* ====================================================================== */
  /* 3D model pipeline resources */
  /* ====================================================================== */
  VkDescriptorSetLayout model_image_set_layout;
  VkDescriptorSetLayout model_ubo_set_layout;
  VkPipelineLayout model_pipeline_layout;
  VkPipeline model_pbr_pipeline;
  /* ====================================================================== */
};

struct owl_vk_config;

enum owl_code owl_init_vk_renderer(struct owl_vk_config const *config,
                                   struct owl_vk_renderer *renderer);

enum owl_code owl_reinit_vk_swapchain(struct owl_vk_config const *config,
                                      struct owl_vk_renderer *renderer);

void owl_deinit_vk_renderer(struct owl_vk_renderer *renderer);

enum owl_code owl_reserve_dyn_buf_mem(struct owl_vk_renderer *renderer,
                                      OwlVkDeviceSize size);

void owl_clear_dyn_garbage(struct owl_vk_renderer *renderer);

int owl_is_dyn_buf_flushed(struct owl_vk_renderer *renderer);

void owl_flush_dyn_buf(struct owl_vk_renderer *renderer);

#endif


#ifndef OWL_RENDERER_INL_
#define OWL_RENDERER_INL_

#include <owl/fwd.h>
#include <owl/memory.h>
#include <owl/pipelines.h>
#include <owl/renderer.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <owl/vkutil.h>
#include <vulkan/vulkan.h>

#define OWL_MAX_DEVICE_OPTIONS 8
#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_DYN_BUF_COUNT 2
#define OWL_MAX_GARBAGE_ITEMS 8

struct owl_vk_surface_provider {
  void const *data;
  enum owl_code (*get)(struct owl_renderer const *renderer, void const *data,
                       void *out);
};

struct owl_vk_extensions {
  OwlU32 count;
  char const *const *names;
};

struct owl_renderer {
  OwlU32 active_buf;
  OwlU32 active_img;

  struct owl_extent extent;

  VkInstance instance;

#ifdef OWL_ENABLE_VALIDATION
  VkDebugUtilsMessengerEXT debug;
#endif

  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;
  VkSurfaceCapabilitiesKHR surface_capabilities;

  OwlU32 device_options_count;
  VkPhysicalDevice device_options[OWL_MAX_DEVICE_OPTIONS];
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceMemoryProperties device_mem_properties;
  VkPhysicalDeviceProperties device_properties;
  VkPhysicalDeviceFeatures device_features;
  VkDevice device;

  OwlU32 graphics_family;
  OwlU32 present_family;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSampleCountFlags samples;

  VkRenderPass main_render_pass;

  OwlU32 img_count;
  VkSwapchainKHR swapchain;
  VkExtent2D swapchain_extent;
  VkImage swapchain_imgs[OWL_MAX_SWAPCHAIN_IMAGES];
  VkImageView swapchain_views[OWL_MAX_SWAPCHAIN_IMAGES];

  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];

  VkClearValue clear_vals[2];

  VkImage color_img;
  VkImageView color_view;
  VkDeviceMemory color_mem;

  VkImage depth_img;
  VkImageView depth_view;
  VkDeviceMemory depth_mem;

  VkCommandPool cmd_pool;
  VkDescriptorPool set_pool;

  VkDescriptorSetLayout uniform_set_layout;
  VkDescriptorSetLayout texture_set_layout;

  VkPipelineLayout main_pipeline_layout;

  VkShaderModule basic_vert_shader;
  VkShaderModule basic_frag_shader;
  VkShaderModule font_frag_shader;

  VkSampler samplers[OWL_SAMPLER_TYPE_COUNT];

  enum owl_pipeline_type bound_pipeline;
  VkPipeline pipelines[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout pipeline_layouts[OWL_PIPELINE_TYPE_COUNT];

  VkCommandPool draw_cmd_pools[OWL_DYN_BUF_COUNT];
  VkCommandBuffer draw_cmd_bufs[OWL_DYN_BUF_COUNT];

  void *dyn_data;
  VkDeviceMemory dyn_mem;
  VkDeviceSize dyn_buf_size;
  VkDeviceSize dyn_aligned_size;
  VkDeviceSize dyn_alignment;
  VkBuffer dyn_bufs[OWL_DYN_BUF_COUNT];
  VkDescriptorSet dyn_sets[OWL_DYN_BUF_COUNT];
  VkDeviceSize dyn_offsets[OWL_DYN_BUF_COUNT];

  OwlU32 dyn_garbage_buf_count;
  OwlU32 dyn_garbage_set_count;
  OwlU32 dyn_garbage_mem_count;
  VkDeviceMemory dyn_garbage_mems[OWL_MAX_GARBAGE_ITEMS];
  VkBuffer dyn_garbage_bufs[OWL_MAX_GARBAGE_ITEMS];
  VkDescriptorSet dyn_garbage_sets[OWL_MAX_GARBAGE_ITEMS];

  VkSemaphore img_available_sema[OWL_DYN_BUF_COUNT];
  VkSemaphore renderer_finished_sema[OWL_DYN_BUF_COUNT];
  VkFence in_flight_fence[OWL_DYN_BUF_COUNT];
};

enum owl_code
owl_init_renderer(struct owl_extent const *extent,
                  struct owl_vk_surface_provider const *provider,
                  struct owl_vk_extensions const *extensions,
                  struct owl_renderer *renderer);

void owl_deinit_renderer(struct owl_renderer *renderer);

enum owl_code owl_reinit_renderer(struct owl_extent const *extent,
                                  struct owl_renderer *renderer);

/* makes sure the size fits on the current buffer */
enum owl_code owl_reserve_dyn_buf_mem(struct owl_renderer *renderer,
                                      OwlDeviceSize size);

void owl_clear_garbage(struct owl_renderer *renderer);

#endif

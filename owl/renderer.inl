
#ifndef OWL_RENDERER_INL_
#define OWL_RENDERER_INL_

#include <owl/pipelines.h>
#include <owl/renderer.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

#define OWL_MAX_PHYSICAL_DEVICE_OPTIONS 8
#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_DYNAMIC_BUFFER_COUNT 2
#define OWL_MAX_GARBAGE_ITEMS 8

typedef uint32_t OtterVkQueueFamily;
typedef VkDeviceSize OwlDeviceSize;

struct owl_renderer;

struct owl_vk_surface_provider {
  void const *data;
  enum owl_code (*get)(struct owl_renderer const *renderer, void const *data,
                       void *out);
};

struct owl_vk_extensions {
  int count;
  char const *const *names;
};

struct owl_vk_pipeline {
  enum owl_pipeline_type bound;
  VkPipeline as[OWL_PIPELINE_TYPE_COUNT];
  VkPipelineLayout layout[OWL_PIPELINE_TYPE_COUNT];
};

struct owl_vk_sampler {
  VkSampler as[OWL_SAMPLER_TYPE_COUNT];
};

enum owl_vk_queue_type {
  OWL_VK_QUEUE_TYPE_GRAPHICS,
  OWL_VK_QUEUE_TYPE_PRESENT,
  OWL_VK_QUEUE_TYPE_COUNT
};

struct owl_vk_device {
  int options_count;
  VkDevice logical;
  VkPhysicalDevice physical;
  VkQueue queues[OWL_VK_QUEUE_TYPE_COUNT];
  OtterVkQueueFamily families[OWL_VK_QUEUE_TYPE_COUNT];
  VkPhysicalDevice options[OWL_MAX_PHYSICAL_DEVICE_OPTIONS];
  VkPhysicalDeviceFeatures features;
  VkPhysicalDeviceProperties props;
  VkPhysicalDeviceMemoryProperties mem_props;
};

struct owl_vk_swapchain {
  OwlU32 img_count;
  OwlU32 current;
  VkSwapchainKHR handle;
  VkExtent2D extent;
  VkClearValue clear_vals[2];
  VkImage imgs[OWL_MAX_SWAPCHAIN_IMAGES];
  VkImageView views[OWL_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];
};

struct owl_vk_attachment {
  VkImage img;
  VkImageView view;
  VkDeviceMemory mem;
};

struct owl_vk_cmd {
  int active;
  VkCommandBuffer bufs[OWL_DYNAMIC_BUFFER_COUNT];
  VkCommandPool pools[OWL_DYNAMIC_BUFFER_COUNT];
};

struct owl_vk_dyn_buf {
  int active;
  void *data;
  VkDeviceSize size;
  VkDeviceSize alignment;
  VkDeviceSize aligned_size;
  VkDeviceMemory mem;
  VkBuffer bufs[OWL_DYNAMIC_BUFFER_COUNT];
  VkDescriptorSet sets[OWL_DYNAMIC_BUFFER_COUNT];
  VkDeviceSize offsets[OWL_DYNAMIC_BUFFER_COUNT];
};

struct owl_vk_garbage {
  int buf_count;
  int set_count;
  int mem_count;
  VkBuffer bufs[OWL_MAX_GARBAGE_ITEMS];
  VkDeviceMemory mems[OWL_MAX_GARBAGE_ITEMS];
  VkDescriptorSet sets[OWL_MAX_GARBAGE_ITEMS];
};

struct owl_vk_frame_sync {
  VkSemaphore img_available[OWL_DYNAMIC_BUFFER_COUNT];
  VkSemaphore render_finished[OWL_DYNAMIC_BUFFER_COUNT];
  VkFence in_flight[OWL_DYNAMIC_BUFFER_COUNT];
};

struct owl_vk_support {
  VkSurfaceFormatKHR format;
  VkSampleCountFlags samples;
  VkSurfaceCapabilitiesKHR capabilities;
};

struct owl_renderer {
  VkInstance instance;
  VkSurfaceKHR surface;
  VkRenderPass main_pass;
  VkCommandPool transient_pool;
  VkDescriptorPool set_pool;
  VkDescriptorSetLayout uniform_layout;
  VkDescriptorSetLayout texture_layout;
  VkPipelineLayout main_layout;
  VkShaderModule basic_vertex;
  VkShaderModule basic_fragment;
  VkShaderModule font_fragment;
#ifdef OWL_ENABLE_VALIDATION
  VkDebugUtilsMessengerEXT debug;
#endif
  struct owl_extent extent;
  struct owl_vk_sampler sampler;
  struct owl_vk_attachment color_attach;
  struct owl_vk_attachment depth_attach;
  struct owl_vk_pipeline pipeline;
  struct owl_vk_frame_sync sync;
  struct owl_vk_support support;
  struct owl_vk_cmd cmd;
  struct owl_vk_garbage garbage;
  struct owl_vk_dyn_buf dyn_buf;
  struct owl_vk_swapchain swapchain;
  struct owl_vk_device device;
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

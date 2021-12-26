
#ifndef OWL_RENDERER_INL_
#define OWL_RENDERER_INL_

#include <owl/fwd.h>
#include <owl/pipelines.h>
#include <owl/renderer.h>
#include <owl/texture.h>
#include <owl/types.h>
#include <vulkan/vulkan.h>

#define OWL_MAX_DEVICE_OPTIONS 8
#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_DYN_BUF_COUNT 2
#define OWL_MAX_GARBAGE_ITEMS 8
#define OWL_CLEAR_VAL_COUNT 2
#define OWL_MB (1024 * 1024)
#define OWL_VK_MEMORY_TYPE_NONE (OwlU32) - 1
#define OWL_VK_DEVICE_EXTENSIONS                                             \
  { VK_KHR_SWAPCHAIN_EXTENSION_NAME }

#define OWL_ALIGN(v, alignment) ((v + alignment - 1) & ~(alignment - 1))

#ifndef NDEBUG
#define OWL_VK_CHECK(e)                                                      \
  do {                                                                       \
    VkResult const result_ = e;                                              \
    OWL_ASSERT(VK_SUCCESS == result_);                                       \
  } while (0)
#else
#define OWL_VK_CHECK(e) e
#endif

#define OWL_VK_DEVICE_EXTENSIONS                                             \
  { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
#define OWL_MB (1024 * 1024)
#define OWL_VK_MEMORY_TYPE_NONE (OwlU32) - 1
#define OWL_ALIGN(v, alignment) ((v + alignment - 1) & ~(alignment - 1))

typedef OwlU32 OwlVkMemoryType;
typedef OwlU32 OwlVkMemoryFilter;
typedef OwlU32 OwlVkQueueFamily;
typedef VkDeviceSize OwlDeviceSize;

enum owl_vk_mem_visibility {
  OWL_VK_MEMORY_VISIBILITY_CPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_GPU_ONLY,
  OWL_VK_MEMORY_VISIBILITY_CPU_TO_GPU
};

typedef enum owl_code (*OwlVkGetSurfaceCb)(
    struct owl_renderer const *renderer, void const *data, void *out);

struct owl_vk_config {
  void const *surface_data;

  OwlU32 extension_count;
  char const *const *extensions;

  OwlVkGetSurfaceCb get_surface;
};

struct owl_renderer {
  /* ====================================================================== */
  /* misc */
  /* ====================================================================== */
  struct owl_extent extent;
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
  VkSurfaceCapabilitiesKHR surface_capabilities;
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
  /* offscreen render pass resources */
  /* ====================================================================== */
  VkRenderPass offscreen_render_pass;

  VkImage offscreen_pos_img;
  VkDeviceMemory offscreen_pos_mem;
  VkImageView offscreen_pos_view;
  VkFormat offscreen_pos_fmt;

  VkImage offscreen_normal_img;
  VkDeviceMemory offscreen_normal_mem;
  VkImageView offscreen_normal_view;
  VkFormat offscreen_normal_fmt;

  VkImage offscreen_albedo_img;
  VkDeviceMemory offscreen_albedo_mem;
  VkImageView offscreen_albedo_view;
  VkFormat offscreen_albedo_fmt;

  VkImage offscreen_depth_img;
  VkDeviceMemory offscreen_depth_mem;
  VkImageView offscreen_depth_view;
  VkFormat offscreen_depth_fmt;

  VkCommandPool offscreen_cmd_pool;
  VkCommandBuffer offscreen_cmd_buf;
  VkFramebuffer offscreen_framebuffer;
  /* ====================================================================== */
};

enum owl_code owl_init_renderer(struct owl_extent const *extent,
                                struct owl_vk_config const *plataform,
                                struct owl_renderer *renderer);

/* frees all resources used by the renderer (not the renderer itself) */
void owl_deinit_renderer(struct owl_renderer *renderer);

/* reinits required resources after a framebuffer size change */
enum owl_code owl_reinit_renderer(struct owl_extent const *extent,
                                  struct owl_renderer *renderer);

/* makes sure the size fits on the current buffer */
enum owl_code owl_reserve_dyn_buf_mem(struct owl_renderer *renderer,
                                      OwlDeviceSize size);

/* clear the accumulated garbage of the dynamic buffer */
void owl_clear_garbage(struct owl_renderer *renderer);

/* find the memory type used by vulkan given the visiblity and filter */
OwlVkMemoryType owl_vk_find_mem_type(struct owl_renderer const *renderer,
                                     OwlVkMemoryFilter filter,
                                     enum owl_vk_mem_visibility visibility);

#endif

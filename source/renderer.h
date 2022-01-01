#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include <owl/owl.h>
#include <vulkan/vulkan.h>

#define OWL_VK_MEMORY_TYPE_NONE (OwlU32) - 1
#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_DYN_BUF_COUNT 2
#define OWL_MAX_GARBAGE_ITEMS 8
#define OWL_MAX_DEVICE_OPTIONS 8
#define OWL_CLEAR_VAL_COUNT 2

enum owl_vk_mem_vis {
  OWL_VK_MEMORY_VIS_CPU_ONLY,
  OWL_VK_MEMORY_VIS_GPU_ONLY,
  OWL_VK_MEMORY_VIS_CPU_TO_GPU
};

struct owl_vk_plataform {
  OwlU32 framebuffer_width;
  OwlU32 framebuffer_height;

  OwlU32 instance_extension_count;
  char const *const *instance_extensions;

  void const *surface_user_data;
  enum owl_code (*create_surface)(struct owl_vk_renderer const *, void const *,
                                  VkSurfaceKHR *);
};

struct owl_dyn_buf_ref {
  OwlU32 offset32;
  VkDeviceSize offset;
  VkBuffer buf;
  VkDescriptorSet pvm_set;
};

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
  /* sampling */
  /* ====================================================================== */
  VkSampleCountFlags samples;
  /* ====================================================================== */

  /* ====================================================================== */
  /* device */
  /* ====================================================================== */
  VkDevice device;
  VkPhysicalDevice physical_device;

  OwlU32 graphics_family;
  VkQueue graphics_queue;

  OwlU32 present_family;
  VkQueue present_queue;

  OwlU32 device_options_count;
  VkPhysicalDevice device_options[OWL_MAX_DEVICE_OPTIONS];

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
  /* main render pass  */
  /* ====================================================================== */
  VkRenderPass main_render_pass;
  /* ====================================================================== */

  /* ====================================================================== */
  /* color attachment */
  /* ====================================================================== */
  VkImage color_img;
  VkImageView color_view;
  VkDeviceMemory color_mem;

  /* ====================================================================== */
  /* depth attachment */
  /* ====================================================================== */
  VkImage depth_img;
  VkImageView depth_view;
  VkDeviceMemory depth_mem;

  /* ====================================================================== */
  /* main framebuffers */
  /* ====================================================================== */
  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];
  /* ====================================================================== */

  /* ====================================================================== */
  /* main sets */
  /* ====================================================================== */
  VkDescriptorSetLayout pvm_set_layout;
  VkDescriptorSetLayout tex_set_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* main pipeline layout */
  /* ====================================================================== */
  VkPipelineLayout main_pipeline_layout;
  /* ====================================================================== */

  /* ====================================================================== */
  /* shaders */
  /* ====================================================================== */
  VkShaderModule basic_vert_shader;
  VkShaderModule basic_frag_shader;
  VkShaderModule font_frag_shader;
  /* ====================================================================== */

  /* ====================================================================== */
  /* pipelines */
  /* ====================================================================== */
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
};

enum owl_code owl_init_vk_renderer(struct owl_vk_plataform const *plataform,
                                   struct owl_vk_renderer *renderer);

void owl_deinit_vk_renderer(struct owl_vk_renderer *renderer);

enum owl_code owl_reinit_vk_swapchain(struct owl_vk_plataform const *plataform,
                                      struct owl_vk_renderer *renderer);

enum owl_code owl_reserve_dyn_buf_mem(struct owl_vk_renderer *renderer,
                                      VkDeviceSize required);

int owl_is_dyn_buf_flushed(struct owl_vk_renderer *renderer);

void owl_flush_dyn_buf(struct owl_vk_renderer *renderer);

void owl_clear_dyn_buf_garbage(struct owl_vk_renderer *renderer);

OwlU32 owl_find_vk_mem_type(struct owl_vk_renderer const *renderer,
                            OwlU32 filter, enum owl_vk_mem_vis vis);

void *owl_alloc_tmp_frame_mem(struct owl_vk_renderer *renderer,
                              VkDeviceSize size, struct owl_dyn_buf_ref *ref);

#endif

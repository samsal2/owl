#ifndef OWL_VK_RENDERER_H_
#define OWL_VK_RENDERER_H_

#include "owl-definitions.h"
#include "owl-vk-font.h"
#include "owl-vk-pipeline.h"
#include "owl-vk-texture.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

#define OWL_MAX_SWAPCHAIN_IMAGES 8
#define OWL_MAX_GARBAGE_ITEMS 8
#define OWL_FONT_FIRST_CHAR ((int32_t)(' '))
#define OWL_FONT_NUM_CHARS ((int32_t)('~' - ' '))

#define OWL_DEF_FRAME_GARBAGE_ARRAY(type, name)                               \
  type name[OWL_MAX_SWAPCHAIN_IMAGES][OWL_MAX_GARBAGE_ITEMS]

struct owl_plataform;

/**
 * @brief global vulkan structure with everything required for renderering
 *
 */
struct owl_vk_renderer {
  /* current plataform resources */
  struct owl_plataform *plataform;

  /** projection matrix used for pipelines that require a pvm */
  owl_m4 projection;
  /** view matrix used for pipelines that required a pvm*/
  owl_m4 view;

  /** framebuffer width in pixels */
  int32_t width;
  /** framebuffer height in pixels */
  int32_t height;

  /** current vulkan instance */
  VkInstance instance;
  /** debug messenger used when validation is enabled */
  VkDebugUtilsMessengerEXT debug_messenger;

  /** current window surface */
  VkSurfaceKHR surface;
  /** current surface format */
  VkSurfaceFormatKHR surface_format;

  /** physical device utilized for rendering */
  VkPhysicalDevice physical_device;
  /** logical device utilized for rendering */
  VkDevice device;
  /** graphics queue family index */
  uint32_t graphics_queue_family;
  /** present queue family index */
  uint32_t present_queue_family;
  /** graphics queue for frame and upload submition */
  VkQueue graphics_queue;
  /** present queue used to present the swapchain image */
  VkQueue present_queue;

  /** current multisampling value */
  VkSampleCountFlagBits msaa;

  /** color attachment image */
  VkImage color_image;
  /** color attachment memory */
  VkDeviceMemory color_memory;
  /** color attachment image view */
  VkImageView color_image_view;

  /** depth stencil attachment format */
  VkFormat depth_format;
  /** depth stencil attachment image */
  VkImage depth_image;
  /** depth stencil attachment memory */
  VkDeviceMemory depth_memory;
  /** depth stencil attachment image view */
  VkImageView depth_image_view;

  /** main render pass  */
  VkRenderPass main_render_pass;
  /** immidiate command buffer used to upload resources  */
  VkCommandBuffer im_command_buffer;

  /** present mode utlized (dictates if vsync is enabled) */
  VkPresentModeKHR present_mode;

  /** vulkan swapchain resources */
  VkSwapchainKHR swapchain;
  /** swapchain width in pixels */
  uint32_t swapchain_width;
  /** swapchain height in pixels */
  uint32_t swapchain_height;
  /** current acquired swapchain image index */
  uint32_t swapchain_image;
  /** number of swapchain images created */
  uint32_t num_swapchain_images;
  /** swapchain images */
  VkImage swapchain_images[OWL_MAX_SWAPCHAIN_IMAGES];
  /** swapchain image attachments */
  VkImageView swapchain_image_views[OWL_MAX_SWAPCHAIN_IMAGES];
  /** main swapchain framebuffers */
  VkFramebuffer swapchain_framebuffers[OWL_MAX_SWAPCHAIN_IMAGES];
  /** clear valures for each submition*/
  VkClearValue swapchain_clear_values[2];

  /** general purpose transient command pool */
  VkCommandPool transient_command_pool;
  /** global descriptor pool */
  VkDescriptorPool descriptor_pool;

  /** basic vertex shader, used by the basic, wires and text pipelines */
  VkShaderModule basic_vertex_shader;
  /** basic fragment shader, used by the basic and wires pipelines */
  VkShaderModule basic_fragment_shader;
  /** text fragment shader, expects an R8_UNORM texture */
  VkShaderModule text_fragment_shader;
  /** model vertex shader */
  VkShaderModule model_vertex_shader;
  /** model fragment shader */
  VkShaderModule model_fragment_shader;
  /** skybox vertex shader */
  VkShaderModule skybox_vertex_shader;
  /** skybox framgnet shader */
  VkShaderModule skybox_fragment_shader;

  /** layout for a dynamic ubo on the vertex stage */
  VkDescriptorSetLayout ubo_vertex_descriptor_set_layout;
  /** layout for a dynamic ubo on the fragment stage */
  VkDescriptorSetLayout ubo_fragment_descriptor_set_layout;
  /** layout for a dynamic ubo on the vertex and fragment stage */
  VkDescriptorSetLayout ubo_both_descriptor_set_layout;
  /** layout for a ssbo on the vertex  stage */
  VkDescriptorSetLayout ssbo_vertex_descriptor_set_layout;
  /** layout for a sampler (on binding 0) and an image (on binding 1) */
  VkDescriptorSetLayout image_fragment_descriptor_set_layout;
  /** ubo on set 0, image on set 1 */
  VkPipelineLayout common_pipeline_layout;
  /** TODO(samuel): description */
  VkPipelineLayout model_pipeline_layout;

  /** bound pipeline index */
  enum owl_vk_pipeline pipeline;
  /** available pipelines */
  VkPipeline pipelines[OWL_VK_NUM_PIPELINES];
  /** corresponding pipeline layouts for bindings */
  VkPipelineLayout pipeline_layouts[OWL_VK_NUM_PIPELINES];

  /** current mapped upload data */
  void *upload_heap_data;
  /** upload heap size */
  VkDeviceSize upload_heap_size;
  /** upload heap buffer */
  VkBuffer upload_heap_buffer;
  /** upload heap memory */
  VkDeviceMemory upload_heap_memory;
  /** the upload heap only supports one allocation at a time */
  int32_t upload_heap_in_use;

  /** linear sampler */
  VkSampler linear_sampler;

  /** has a skybox been loaded */
  int32_t skybox_loaded;
  /** skybox image */
  VkImage skybox_image;
  /** skybox memory */
  VkDeviceMemory skybox_memory;
  /** skybox image view */
  VkImageView skybox_image_view;
  /** skybox descriptor set */
  VkDescriptorSet skybox_descriptor_set;

  /** has a font been loaded */
  int32_t font_loaded;
  /** font atlas texture */
  struct owl_vk_texture font_atlas;
  /** font packed chars for glyph generation */
  struct owl_vk_packed_char font_chars[OWL_FONT_NUM_CHARS];

  /** https://arm-software.github.io/vulkan_best_practice_for_mobile_developers/samples/performance/command_buffer_usage/command_buffer_usage_tutorial.html */
  VkCommandPool frame_command_pools[OWL_MAX_SWAPCHAIN_IMAGES];
  /** command buffers recorded every frame */
  VkCommandBuffer frame_command_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  /** in flight fence syncronization primitive */
  VkFence frame_in_flight_fences[OWL_MAX_SWAPCHAIN_IMAGES];
  /** swapchain image available syncronization primitive */
  VkSemaphore frame_image_available_semaphores[OWL_MAX_SWAPCHAIN_IMAGES];
  /** render done syncronization primitive */
  VkSemaphore frame_render_done_semaphores[OWL_MAX_SWAPCHAIN_IMAGES];

  /** TODO(samuel): single memory block for all frame heap buffers */

  /** current frame index */
  uint32_t frame;
  /** frame heap size */
  VkDeviceSize frame_heap_size;
  /** frame heap offset aligned to the frame heap alignment */
  VkDeviceSize frame_heap_offset;
  /** frame heap alignment */
  VkDeviceSize frame_heap_alignment;

  /** mapped frame heap memories */
  void *frame_heap_data[OWL_MAX_SWAPCHAIN_IMAGES];
  /** frame heap buffers for vertex or index submition */
  VkBuffer frame_heap_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  /** frame heap memories */
  VkDeviceMemory frame_heap_memories[OWL_MAX_SWAPCHAIN_IMAGES];
  /** frame heap descriptor set for an owl_pvm_ubo struct */
  VkDescriptorSet frame_heap_pvm_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  /** frame heap descriptor set for an owl_model_ubo1 struct */
  VkDescriptorSet frame_heap_model1_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  /** frame heap descriptor set for an owl_model_ubo2 struct */
  VkDescriptorSet frame_heap_model2_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];

  /** number for frame garbage buffers at a given frame */
  uint32_t num_frame_garbage_buffers[OWL_MAX_SWAPCHAIN_IMAGES];
  /** array of fram garbage buffers at a given frame */
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkBuffer, frame_garbage_buffers);

  /** number for frame garbage memories at a given frame */
  uint32_t num_frame_garbage_memories[OWL_MAX_SWAPCHAIN_IMAGES];
  /** array of frame garbage memories at a given frame */
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkDeviceMemory, frame_garbage_memories);

  /** number for frame garbage descriptor sets at a given frame */
  uint32_t num_frame_garbage_descriptor_sets[OWL_MAX_SWAPCHAIN_IMAGES];
  /** array of frame garbage descriptor sets at a given frame */
  OWL_DEF_FRAME_GARBAGE_ARRAY(VkDescriptorSet, frame_garbage_descriptor_sets);

  PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
  PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
};

/**
 * @brief init a renderer instance
 *
 * @param vk
 * @param plataform
 * @return owl_code
 */
owl_public owl_code
owl_vk_renderer_init(struct owl_vk_renderer *vk,
                     struct owl_plataform *plataform);

/**
 * @brief deinit a renderer instance
 *
 * @param vk
 * @return void
 */
owl_public void
owl_vk_renderer_deinit(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

#ifndef OWL_RENDER_H
#define OWL_RENDER_H

#include <vulkan/vulkan.h>

struct owl_plataform;
struct owl_render;
struct owl_dynamic_buffer;
struct owl_dynamic_buffer_ref;
struct owl_uniform_buffer;
struct owl_uniform_buffer_ref;
struct owl_texture;
struct owl_texture_desc;

#define OWL_EXTERN

OWL_EXTERN int owl_render_init(struct owl_render *render,
    struct owl_plataform *plataform);

OWL_EXTERN void owl_render_deinit(struct owl_render *render);

#define OWL_DYNAMIC_BUFFER_VERTEX 0x0001
#define OWL_DYNAMIC_BUFFER_INDEX 0x0002
#define OWL_DYNAMIC_BUFFER_UPLOAD 0x0004

OWL_EXTERN int owl_render_init_dynamic_buffer(struct owl_render *render,
    uint32_t type, uint64_t size, struct owl_dynamic_buffer *buffer);
OWL_EXTERN int owl_render_deinit_dynamic_buffer(struct owl_render *render,
    struct owl_dynamic_buffer *buffer);
OWL_EXTERN int owl_dynamic_buffer_push(struct owl_dynamic_buffer *buffer,
    struct owl_render *render, uint64_t size);
OWL_EXTERN void owl_dynamic_buffer_pop(struct owl_dynamic_buffer *buffer,
    struct owl_render *render);

OWL_EXTERN int owl_render_init_uniform_buffer(struct owl_render *render,
    uint64_t size, struct owl_uniform_buffer *buffer);
OWL_EXTERN int owl_render_deinit_uniform_buffer(struct owl_render *render,
    struct owl_uniform_buffer *buffer);
OWL_EXTERN int owl_uniform_buffer_push(struct owl_uniform_buffer *buffer,
    struct owl_render *render, uint64_t size);
OWL_EXTERN void owl_uniform_buffer_pop(struct owl_uniform_buffer *buffer,
    struct owl_render *render);

OWL_EXTERN void *owl_render_vertex_allocate(struct owl_render *render,
    uint64_t size, struct owl_dynamic_buffer_ref *ref);
OWL_EXTERN void *owl_render_index_allocate(struct owl_render *render,
    uint64_t size, struct owl_dynamic_buffer_ref *ref);
OWL_EXTERN void *owl_render_uniform_allocate(struct owl_render *render,
    uint64_t size, struct owl_uniform_buffer *ref);
OWL_EXTERN void *owl_render_upload_allocate(struct owl_render *render,
    uint64_t size, struct owl_dynamic_buffer_ref *ref);

OWL_EXTERN int owl_render_init_texture(struct owl_render *render,
    struct owl_texture_desc *desc, struct owl_texture *texture);
OWL_EXTERN int owl_render_deinit_texture(struct owl_render *render,
    struct owl_texture_desc *desc, struct owl_texture *texture);

OWL_EXTERN int owl_render_begin_frame(struct owl_render *render);
OWL_EXTERN int owl_render_end_frame(struct owl_render *render);

OWL_EXTERN int owl_render_begin_immediate(struct owl_render *render);
OWL_EXTERN int owl_render_end_immediate(struct owl_render *render);

#define OWL_TEXTURE_2D 0
#define OWL_TEXTURE_CUBE 1

struct owl_texture {
  uint32_t width;
  uint32_t height;
  uint32_t mipmaps;
  uint32_t layers;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkDescriptorSet set;
  VkImageLayout layout;
};

struct owl_dynamic_buffer_slot {
  VkBuffer buffer;
  VkDeviceMemory memory;
};

struct owl_dynamic_buffer_ref {
  uint64_t offset;
  struct owl_dynamic_buffer_slot *slot;
};

#define OWL_DYNAMIC_BUFFER_SLOT_COUNT 8

struct owl_dynamic_buffer {
  int32_t start;
  int32_t end;
  uint64_t size;
  uint64_t offset;
  uint64_t alignment;
  struct owl_dynamic_buffer_slot slots[OWL_DYNAMIC_BUFFER_SLOT_COUNT];
};

struct owl_uniform_buffer_slot {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDescriptorSet pvm_set;
  VkDescriptorSet model1_set;
  VkDescriptorSet model2_set;
};

struct owl_uniform_buffer_ref {
  uint32_t offset;
  struct owl_uniform_buffer_slot *slot;
};

#define OWL_UNIFORM_BUFFER_SLOT_COUNT 8

struct owl_uniform_buffer {
  int32_t start;
  int32_t end;
  uint32_t size;
  uint32_t offset;
  uint32_t alignment;
  struct owl_uniform_buffer_slot slots[OWL_UNIFORM_BUFFER_SLOT_COUNT];
};

#define OWL_MAX_SWAPCHAIN_IMAGE_COUNT 8
#define OWL_IN_FLIGHT_FRAME_COUNT 2

struct owl_framebuffer_attachment {
  VkFormat format;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
};

struct owl_render {
  uint32_t width;
  uint32_t height;
  VkInstance instance;
  VkSurfaceKHR surface;
  VkSurfaceFormatKHR surface_format;
  VkFormat depth_format;
  VkPhysicalDevice physical_device;
  VkDevice device;
  uint32_t graphics_family;
  uint32_t present_family;
  VkQueue graphics_queue;
  VkQueue present_queue;
  struct owl_framebuffer_attachment color_attachment;
  struct owl_framebuffer_attachment depth_attachment;
  VkSwapchainKHR swapchain;
  uint32_t image;
  uint32_t image_count;
  VkRenderPass main_pass;
  VkImageView swapchain_views[OWL_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkFramebuffer framebuffers[OWL_MAX_SWAPCHAIN_IMAGE_COUNT];
  VkDescriptorPool descriptor_pool;
  VkCommandPool command_pool;
  VkDescriptorSetLayout ubo_vert_layout;
  VkDescriptorSetLayout ubo_frag_layout;
  VkDescriptorSetLayout ubo_both_layout;
  VkDescriptorSetLayout ssbo_vert_layout;
  VkDescriptorSetLayout image_frag_layout;
  VkPipelineLayout common_layout;
  VkPipelineLayout model_layout;
  VkPipeline basic_pipeline;
  VkPipeline wires_pipeline;
  VkPipeline text_pipeline;
  VkPipeline model_pipeline;
  VkPipeline skybox_pipeline;
  VkCommandBuffer immidiate;
  VkSampler linear_sampler;
  VkCommandPool submit_command_pool[OWL_IN_FLIGHT_FRAME_COUNT];
  VkCommandPool submit_command_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
  VkFence in_flight_fence[OWL_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore acquire_semaphore[OWL_IN_FLIGHT_FRAME_COUNT];
  VkSemaphore render_done_semaphore[OWL_IN_FLIGHT_FRAME_COUNT];
  uint32_t frame;
  int32_t font_loaded;
  struct owl_texture font_atlas;
  int32_t skybox_loaded;
  struct owl_texture skybox;
  struct owl_dynamic_buffer upload_buffer;
  struct owl_dynamic_buffer submit_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
  struct owl_uniform_buffer uniform_buffers[OWL_IN_FLIGHT_FRAME_COUNT];
};

#endif

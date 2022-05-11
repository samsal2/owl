#ifndef OWL_VK_PIPELINE_MANAGER_H_
#define OWL_VK_PIPELINE_MANAGER_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_swapchain;
struct owl_vk_frame;

enum owl_pipeline_id {
  OWL_PIPELINE_ID_BASIC,
  OWL_PIPELINE_ID_WIRES,
  OWL_PIPELINE_ID_TEXT,
  OWL_PIPELINE_ID_MODEL,
  OWL_PIPELINE_ID_COUNT,
  OWL_PIPELINE_ID_NONE = OWL_PIPELINE_ID_COUNT
};

struct owl_vk_pipeline_manager {
  VkDescriptorSetLayout vk_vert_ubo_set_layout;
  VkDescriptorSetLayout vk_frag_ubo_set_layout;
  VkDescriptorSetLayout vk_both_ubo_set_layout;
  VkDescriptorSetLayout vk_vert_ssbo_set_layout;
  VkDescriptorSetLayout vk_frag_image_set_layout;

  VkShaderModule vk_basic_vert_shader;
  VkShaderModule vk_basic_frag_shader;
  VkShaderModule vk_text_frag_shader;
  VkShaderModule vk_model_vert_shader;
  VkShaderModule vk_model_frag_shader;

  VkPipelineLayout vk_common_pipeline_layout;
  VkPipelineLayout vk_model_pipeline_layout;

  enum owl_pipeline_id active_pipeline;

  VkPipeline vk_pipelines[OWL_PIPELINE_ID_COUNT];
  VkPipelineLayout vk_pipeline_layouts[OWL_PIPELINE_ID_COUNT];
};

owl_public enum owl_code
owl_vk_pipeline_manager_init (struct owl_vk_pipeline_manager *pm,
                              struct owl_vk_context *ctx,
                              struct owl_vk_swapchain *swapchain);

owl_public void
owl_vk_pipeline_manager_deinit (struct owl_vk_pipeline_manager *pm,
                                struct owl_vk_context *ctx);

owl_public enum owl_code
owl_vk_pipeline_manager_bind (struct owl_vk_pipeline_manager *pm,
                              enum owl_pipeline_id id,
                              struct owl_vk_frame const *frame);

owl_public VkPipelineLayout
owl_vk_pipeline_manager_layout_get (struct owl_vk_pipeline_manager const *pm);

OWL_END_DECLS

#endif

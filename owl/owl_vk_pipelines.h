#ifndef OWL_VK_PIPELINE_MANAGER_H_
#define OWL_VK_PIPELINE_MANAGER_H_

#include "owl_definitions.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_swapchain;
struct owl_vk_frame;

enum owl_vk_pipeline_id
{
  OWL_VK_PIPELINE_ID_BASIC,
  OWL_VK_PIPELINE_ID_WIRES,
  OWL_VK_PIPELINE_ID_TEXT,
  OWL_VK_PIPELINE_ID_MODEL,
  OWL_VK_PIPELINE_ID_SKYBOX,
  OWL_VK_PIPELINE_ID_COUNT,
  OWL_VK_PIPELINE_ID_NONE = OWL_VK_PIPELINE_ID_COUNT
};

struct owl_vk_pipelines
{
  VkShaderModule vk_basic_vert;
  VkShaderModule vk_basic_frag;
  VkShaderModule vk_text_frag;
  VkShaderModule vk_model_vert;
  VkShaderModule vk_model_frag;
  VkShaderModule vk_skybox_vert;
  VkShaderModule vk_skybox_frag;

  VkPipelineLayout vk_common_layout;
  VkPipelineLayout vk_model_layout;

  enum owl_vk_pipeline_id active;

  VkPipeline       vk_pipelines[OWL_VK_PIPELINE_ID_COUNT];
  VkPipelineLayout vk_pipeline_layouts[OWL_VK_PIPELINE_ID_COUNT];
};

owl_public enum owl_code
owl_vk_pipelines_init (struct owl_vk_pipelines *ps,
                       struct owl_vk_context   *ctx,
                       struct owl_vk_swapchain *sc);

owl_public void
owl_vk_pipelines_deinit (struct owl_vk_pipelines *ps,
                         struct owl_vk_context   *ctx);

owl_public enum owl_code
owl_vk_pipelines_bind (struct owl_vk_pipelines   *ps,
                       enum owl_vk_pipeline_id    id,
                       struct owl_vk_frame const *frame);

owl_public VkPipelineLayout
owl_vk_pipelines_get_layout (struct owl_vk_pipelines const *ps);

OWL_END_DECLS

#endif

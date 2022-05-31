#include "owl_vk_pipelines.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"
#include "owl_vk_swapchain.h"
#include "owl_vk_types.h"

owl_private enum owl_code
owl_vk_pipelines_layouts_init (struct owl_vk_pipelines *ps,
                               struct owl_vk_context   *ctx)
{
  VkDescriptorSetLayout      sets[6];
  VkPushConstantRange        range;
  VkPipelineLayoutCreateInfo info;

  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  sets[0] = ctx->vk_ubo_vert_set_layout;
  sets[1] = ctx->vk_image_frag_set_layout;

  info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext                  = NULL;
  info.flags                  = 0;
  info.setLayoutCount         = 2;
  info.pSetLayouts            = sets;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges    = NULL;

  vk_result = vkCreatePipelineLayout (ctx->vk_device, &info, NULL,
                                      &ps->vk_common_pipeline_layout);

  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  range.offset     = 0;
  range.size       = sizeof (struct owl_model_material_push_constant);

  sets[0] = ctx->vk_ubo_both_set_layout;
  sets[1] = ctx->vk_image_frag_set_layout;
  sets[2] = ctx->vk_image_frag_set_layout;
  sets[3] = ctx->vk_image_frag_set_layout;
  sets[4] = ctx->vk_ssbo_vert_set_layout;
  sets[5] = ctx->vk_ubo_frag_set_layout;

  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges    = &range;

  info.setLayoutCount = 6;

  vk_result = vkCreatePipelineLayout (ctx->vk_device, &info, NULL,
                                      &ps->vk_model_pipeline_layout);

  if (VK_SUCCESS != vk_result)
  {
    code = OWL_ERROR_UNKNOWN;
    goto error_model_pipeline_layout_deinit;
  }

  goto out;

error_model_pipeline_layout_deinit:
  vkDestroyPipelineLayout (ctx->vk_device, ps->vk_common_pipeline_layout,
                           NULL);

out:

  return code;
}

owl_private void
owl_vk_pipelines_layouts_deinit (struct owl_vk_pipelines *ps,
                                 struct owl_vk_context   *ctx)
{
  vkDestroyPipelineLayout (ctx->vk_device, ps->vk_model_pipeline_layout, NULL);
  vkDestroyPipelineLayout (ctx->vk_device, ps->vk_common_pipeline_layout,
                           NULL);
}

owl_private enum owl_code
owl_vk_pipelines_shaders_init (struct owl_vk_pipelines *ps,
                               struct owl_vk_context   *ctx)
{
  VkResult      vk_result = VK_SUCCESS;
  enum owl_code code      = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo info;

    /* TODO(samuel): Properly load code at runtime */
    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.vert.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_basic_vert_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.frag.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_basic_frag_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_basic_vert_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_font.frag.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_text_frag_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_basic_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.vert.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_model_vert_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_text_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.frag.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_model_frag_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_model_vert_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.vert.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_skybox_vert_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_model_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.frag.spv.u32"
    };

    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext    = NULL;
    info.flags    = 0;
    info.codeSize = sizeof (spv);
    info.pCode    = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &ps->vk_skybox_frag_shader);

    if (VK_SUCCESS != vk_result)
    {
      code = OWL_ERROR_UNKNOWN;
      goto error_skybox_vert_shader_deinit;
    }
  }

  goto out;

error_skybox_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_skybox_vert_shader, NULL);

error_model_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_model_frag_shader, NULL);

error_model_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_model_vert_shader, NULL);

error_text_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_text_frag_shader, NULL);

error_basic_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_basic_frag_shader, NULL);

error_basic_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, ps->vk_basic_vert_shader, NULL);

out:
  return code;
}

owl_private void
owl_vk_pipelines_shaders_deinit (struct owl_vk_pipelines *ps,
                                 struct owl_vk_context   *ctx)
{
  vkDestroyShaderModule (ctx->vk_device, ps->vk_skybox_frag_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_skybox_vert_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_model_frag_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_model_vert_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_text_frag_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_basic_frag_shader, NULL);
  vkDestroyShaderModule (ctx->vk_device, ps->vk_basic_vert_shader, NULL);
}

owl_private enum owl_code
owl_vk_pipelines_basic_init (struct owl_vk_pipelines       *ps,
                             struct owl_vk_context const   *ctx,
                             struct owl_vk_swapchain const *sc)
{
  VkVertexInputBindingDescription        vert_binding;
  VkVertexInputAttributeDescription      vert_attr[3];
  VkPipelineVertexInputStateCreateInfo   vert_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport                             viewport;
  VkRect2D                               scissor;
  VkPipelineViewportStateCreateInfo      viewport_state;
  VkPipelineRasterizationStateCreateInfo resterization;
  VkPipelineMultisampleStateCreateInfo   multisample;
  VkPipelineColorBlendAttachmentState    color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo    color_blend;
  VkPipelineDepthStencilStateCreateInfo  depth_stencil;
  VkPipelineShaderStageCreateInfo        stages[2];
  VkGraphicsPipelineCreateInfo           info;
  VkResult                               vk_result;

  vert_binding.binding   = 0;
  vert_binding.stride    = sizeof (struct owl_pcu_vertex);
  vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vert_attr[0].binding  = 0;
  vert_attr[0].location = 0;
  vert_attr[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[0].offset   = offsetof (struct owl_pcu_vertex, position);

  vert_attr[1].binding  = 0;
  vert_attr[1].location = 1;
  vert_attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[1].offset   = offsetof (struct owl_pcu_vertex, color);

  vert_attr[2].binding  = 0;
  vert_attr[2].location = 2;
  vert_attr[2].format   = VK_FORMAT_R32G32_SFLOAT;
  vert_attr[2].offset   = offsetof (struct owl_pcu_vertex, uv);

  vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input.pNext = NULL;
  vert_input.flags = 0;
  vert_input.vertexBindingDescriptionCount   = 1;
  vert_input.pVertexBindingDescriptions      = &vert_binding;
  vert_input.vertexAttributeDescriptionCount = owl_array_size (vert_attr);
  vert_input.pVertexAttributeDescriptions    = vert_attr;

  input_assembly.sType
      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext                  = NULL;
  input_assembly.flags                  = 0;
  input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x        = 0.0F;
  viewport.y        = 0.0F;
  viewport.width    = sc->extent.width;
  viewport.height   = sc->extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent   = sc->extent;

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &scissor;

  resterization.sType
      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  resterization.pNext                   = NULL;
  resterization.flags                   = 0;
  resterization.depthClampEnable        = VK_FALSE;
  resterization.rasterizerDiscardEnable = VK_FALSE;
  resterization.polygonMode             = VK_POLYGON_MODE_FILL;
  resterization.cullMode                = VK_CULL_MODE_BACK_BIT;
  resterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  resterization.depthBiasEnable         = VK_FALSE;
  resterization.depthBiasConstantFactor = 0.0F;
  resterization.depthBiasClamp          = 0.0F;
  resterization.depthBiasSlopeFactor    = 0.0F;
  resterization.lineWidth               = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = NULL;
  multisample.flags = 0;
  multisample.rasterizationSamples  = ctx->vk_msaa;
  multisample.sampleShadingEnable   = VK_FALSE;
  multisample.minSampleShading      = 1.0F;
  multisample.pSampleMask           = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable      = VK_FALSE;

  color_blend_attachment.blendEnable         = VK_FALSE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable     = VK_FALSE;
  color_blend.logicOp           = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount   = 1;
  color_blend.pAttachments      = &color_blend_attachment;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType
      = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext                 = NULL;
  depth_stencil.flags                 = 0;
  depth_stencil.depthTestEnable       = VK_TRUE;
  depth_stencil.depthWriteEnable      = VK_TRUE;
  depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
  owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext  = NULL;
  stages[0].flags  = 0;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = ps->vk_basic_vert_shader;
  stages[0].pName  = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext  = NULL;
  stages[1].flags  = 0;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps->vk_basic_frag_shader;
  stages[1].pName  = "main";
  stages[1].pSpecializationInfo = NULL;

  ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_BASIC]
      = ps->vk_common_pipeline_layout;

  info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext               = NULL;
  info.flags               = 0;
  info.stageCount          = owl_array_size (stages);
  info.pStages             = stages;
  info.pVertexInputState   = &vert_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState  = NULL;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &resterization;
  info.pMultisampleState   = &multisample;
  info.pDepthStencilState  = &depth_stencil;
  info.pColorBlendState    = &color_blend;
  info.pDynamicState       = NULL;
  info.layout              = ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_BASIC];
  info.renderPass          = ctx->vk_main_render_pass;
  info.subpass             = 0;
  info.basePipelineHandle  = VK_NULL_HANDLE;
  info.basePipelineIndex   = -1;

  /* TODO(samuel): create all pipelines in a single batch */
  vk_result = vkCreateGraphicsPipelines (
      ctx->vk_device, VK_NULL_HANDLE, 1, &info, NULL,
      &ps->vk_pipelines[OWL_VK_PIPELINE_ID_BASIC]);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_pipelines_basic_deinit (struct owl_vk_pipelines     *ps,
                               struct owl_vk_context const *ctx)
{
  vkDestroyPipeline (ctx->vk_device,
                     ps->vk_pipelines[OWL_VK_PIPELINE_ID_BASIC], NULL);
}

owl_private enum owl_code
owl_vk_pipelines_wires_init (struct owl_vk_pipelines       *ps,
                             struct owl_vk_context const   *ctx,
                             struct owl_vk_swapchain const *sc)
{
  VkVertexInputBindingDescription        vert_binding;
  VkVertexInputAttributeDescription      vert_attr[3];
  VkPipelineVertexInputStateCreateInfo   vert_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport                             viewport;
  VkRect2D                               scissor;
  VkPipelineViewportStateCreateInfo      viewport_state;
  VkPipelineRasterizationStateCreateInfo resterization;
  VkPipelineMultisampleStateCreateInfo   multisample;
  VkPipelineColorBlendAttachmentState    color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo    color_blend;
  VkPipelineDepthStencilStateCreateInfo  depth_stencil;
  VkPipelineShaderStageCreateInfo        stages[2];
  VkGraphicsPipelineCreateInfo           info;
  VkResult                               vk_result;

  vert_binding.binding   = 0;
  vert_binding.stride    = sizeof (struct owl_pcu_vertex);
  vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vert_attr[0].binding  = 0;
  vert_attr[0].location = 0;
  vert_attr[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[0].offset   = offsetof (struct owl_pcu_vertex, position);

  vert_attr[1].binding  = 0;
  vert_attr[1].location = 1;
  vert_attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[1].offset   = offsetof (struct owl_pcu_vertex, color);

  vert_attr[2].binding  = 0;
  vert_attr[2].location = 2;
  vert_attr[2].format   = VK_FORMAT_R32G32_SFLOAT;
  vert_attr[2].offset   = offsetof (struct owl_pcu_vertex, uv);

  vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input.pNext = NULL;
  vert_input.flags = 0;
  vert_input.vertexBindingDescriptionCount   = 1;
  vert_input.pVertexBindingDescriptions      = &vert_binding;
  vert_input.vertexAttributeDescriptionCount = owl_array_size (vert_attr);
  vert_input.pVertexAttributeDescriptions    = vert_attr;

  input_assembly.sType
      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext                  = NULL;
  input_assembly.flags                  = 0;
  input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x        = 0.0F;
  viewport.y        = 0.0F;
  viewport.width    = sc->extent.width;
  viewport.height   = sc->extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent   = sc->extent;

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &scissor;

  resterization.sType
      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  resterization.pNext                   = NULL;
  resterization.flags                   = 0;
  resterization.depthClampEnable        = VK_FALSE;
  resterization.rasterizerDiscardEnable = VK_FALSE;
  resterization.polygonMode             = VK_POLYGON_MODE_LINE;
  resterization.cullMode                = VK_CULL_MODE_BACK_BIT;
  resterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  resterization.depthBiasEnable         = VK_FALSE;
  resterization.depthBiasConstantFactor = 0.0F;
  resterization.depthBiasClamp          = 0.0F;
  resterization.depthBiasSlopeFactor    = 0.0F;
  resterization.lineWidth               = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = NULL;
  multisample.flags = 0;
  multisample.rasterizationSamples  = ctx->vk_msaa;
  multisample.sampleShadingEnable   = VK_FALSE;
  multisample.minSampleShading      = 1.0F;
  multisample.pSampleMask           = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable      = VK_FALSE;

  color_blend_attachment.blendEnable         = VK_FALSE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable     = VK_FALSE;
  color_blend.logicOp           = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount   = 1;
  color_blend.pAttachments      = &color_blend_attachment;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType
      = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext                 = NULL;
  depth_stencil.flags                 = 0;
  depth_stencil.depthTestEnable       = VK_TRUE;
  depth_stencil.depthWriteEnable      = VK_TRUE;
  depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
  owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext  = NULL;
  stages[0].flags  = 0;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = ps->vk_basic_vert_shader;
  stages[0].pName  = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext  = NULL;
  stages[1].flags  = 0;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps->vk_basic_frag_shader;
  stages[1].pName  = "main";
  stages[1].pSpecializationInfo = NULL;

  ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_WIRES]
      = ps->vk_common_pipeline_layout;

  info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext               = NULL;
  info.flags               = 0;
  info.stageCount          = owl_array_size (stages);
  info.pStages             = stages;
  info.pVertexInputState   = &vert_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState  = NULL;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &resterization;
  info.pMultisampleState   = &multisample;
  info.pDepthStencilState  = &depth_stencil;
  info.pColorBlendState    = &color_blend;
  info.pDynamicState       = NULL;
  info.layout              = ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_WIRES];
  info.renderPass          = ctx->vk_main_render_pass;
  info.subpass             = 0;
  info.basePipelineHandle  = VK_NULL_HANDLE;
  info.basePipelineIndex   = -1;

  /* TODO(samuel): create all pipelines in a single batch */
  vk_result = vkCreateGraphicsPipelines (
      ctx->vk_device, VK_NULL_HANDLE, 1, &info, NULL,
      &ps->vk_pipelines[OWL_VK_PIPELINE_ID_WIRES]);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_pipelines_wires_deinit (struct owl_vk_pipelines     *ps,
                               struct owl_vk_context const *ctx)
{
  vkDestroyPipeline (ctx->vk_device,
                     ps->vk_pipelines[OWL_VK_PIPELINE_ID_WIRES], NULL);
}

owl_private enum owl_code
owl_vk_pipelines_text_init (struct owl_vk_pipelines       *ps,
                            struct owl_vk_context const   *ctx,
                            struct owl_vk_swapchain const *sc)
{
  VkVertexInputBindingDescription        vert_binding;
  VkVertexInputAttributeDescription      vert_attr[3];
  VkPipelineVertexInputStateCreateInfo   vert_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport                             viewport;
  VkRect2D                               scissor;
  VkPipelineViewportStateCreateInfo      viewport_state;
  VkPipelineRasterizationStateCreateInfo resterization;
  VkPipelineMultisampleStateCreateInfo   multisample;
  VkPipelineColorBlendAttachmentState    color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo    color_blend;
  VkPipelineDepthStencilStateCreateInfo  depth_stencil;
  VkPipelineShaderStageCreateInfo        stages[2];
  VkGraphicsPipelineCreateInfo           info;
  VkResult                               vk_result;

  vert_binding.binding   = 0;
  vert_binding.stride    = sizeof (struct owl_pcu_vertex);
  vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vert_attr[0].binding  = 0;
  vert_attr[0].location = 0;
  vert_attr[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[0].offset   = offsetof (struct owl_pcu_vertex, position);

  vert_attr[1].binding  = 0;
  vert_attr[1].location = 1;
  vert_attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[1].offset   = offsetof (struct owl_pcu_vertex, color);

  vert_attr[2].binding  = 0;
  vert_attr[2].location = 2;
  vert_attr[2].format   = VK_FORMAT_R32G32_SFLOAT;
  vert_attr[2].offset   = offsetof (struct owl_pcu_vertex, uv);

  vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input.pNext = NULL;
  vert_input.flags = 0;
  vert_input.vertexBindingDescriptionCount   = 1;
  vert_input.pVertexBindingDescriptions      = &vert_binding;
  vert_input.vertexAttributeDescriptionCount = owl_array_size (vert_attr);
  vert_input.pVertexAttributeDescriptions    = vert_attr;

  input_assembly.sType
      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext                  = NULL;
  input_assembly.flags                  = 0;
  input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x        = 0.0F;
  viewport.y        = 0.0F;
  viewport.width    = sc->extent.width;
  viewport.height   = sc->extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent   = sc->extent;

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &scissor;

  resterization.sType
      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  resterization.pNext                   = NULL;
  resterization.flags                   = 0;
  resterization.depthClampEnable        = VK_FALSE;
  resterization.rasterizerDiscardEnable = VK_FALSE;
  resterization.polygonMode             = VK_POLYGON_MODE_FILL;
  resterization.cullMode                = VK_CULL_MODE_BACK_BIT;
  resterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  resterization.depthBiasEnable         = VK_FALSE;
  resterization.depthBiasConstantFactor = 0.0F;
  resterization.depthBiasClamp          = 0.0F;
  resterization.depthBiasSlopeFactor    = 0.0F;
  resterization.lineWidth               = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = NULL;
  multisample.flags = 0;
  multisample.rasterizationSamples  = ctx->vk_msaa;
  multisample.sampleShadingEnable   = VK_FALSE;
  multisample.minSampleShading      = 1.0F;
  multisample.pSampleMask           = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable      = VK_FALSE;

  color_blend_attachment.blendEnable         = VK_TRUE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstColorBlendFactor
      = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attachment.dstAlphaBlendFactor
      = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable     = VK_FALSE;
  color_blend.logicOp           = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount   = 1;
  color_blend.pAttachments      = &color_blend_attachment;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType
      = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext                 = NULL;
  depth_stencil.flags                 = 0;
  depth_stencil.depthTestEnable       = VK_TRUE;
  depth_stencil.depthWriteEnable      = VK_TRUE;
  depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
  owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext  = NULL;
  stages[0].flags  = 0;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = ps->vk_basic_vert_shader;
  stages[0].pName  = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext  = NULL;
  stages[1].flags  = 0;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps->vk_text_frag_shader;
  stages[1].pName  = "main";
  stages[1].pSpecializationInfo = NULL;

  ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_TEXT]
      = ps->vk_common_pipeline_layout;

  info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext               = NULL;
  info.flags               = 0;
  info.stageCount          = owl_array_size (stages);
  info.pStages             = stages;
  info.pVertexInputState   = &vert_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState  = NULL;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &resterization;
  info.pMultisampleState   = &multisample;
  info.pDepthStencilState  = &depth_stencil;
  info.pColorBlendState    = &color_blend;
  info.pDynamicState       = NULL;
  info.layout              = ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_TEXT];
  info.renderPass          = ctx->vk_main_render_pass;
  info.subpass             = 0;
  info.basePipelineHandle  = VK_NULL_HANDLE;
  info.basePipelineIndex   = -1;

  /* TODO(samuel): create all pipelines in a single batch */
  vk_result = vkCreateGraphicsPipelines (
      ctx->vk_device, VK_NULL_HANDLE, 1, &info, NULL,
      &ps->vk_pipelines[OWL_VK_PIPELINE_ID_TEXT]);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_pipelines_text_deinit (struct owl_vk_pipelines     *ps,
                              struct owl_vk_context const *ctx)
{
  vkDestroyPipeline (ctx->vk_device, ps->vk_pipelines[OWL_VK_PIPELINE_ID_TEXT],
                     NULL);
}

owl_private enum owl_code
owl_vk_pipelines_model_init (struct owl_vk_pipelines       *ps,
                             struct owl_vk_context const   *ctx,
                             struct owl_vk_swapchain const *sc)
{
  VkVertexInputBindingDescription        vert_binding;
  VkVertexInputAttributeDescription      vert_attr[6];
  VkPipelineVertexInputStateCreateInfo   vert_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport                             viewport;
  VkRect2D                               scissor;
  VkPipelineViewportStateCreateInfo      viewport_state;
  VkPipelineRasterizationStateCreateInfo resterization;
  VkPipelineMultisampleStateCreateInfo   multisample;
  VkPipelineColorBlendAttachmentState    color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo    color_blend;
  VkPipelineDepthStencilStateCreateInfo  depth_stencil;
  VkPipelineShaderStageCreateInfo        stages[2];
  VkGraphicsPipelineCreateInfo           info;
  VkResult                               vk_result;

  vert_binding.binding   = 0;
  vert_binding.stride    = sizeof (struct owl_pnuujw_vertex);
  vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vert_attr[0].binding  = 0;
  vert_attr[0].location = 0;
  vert_attr[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[0].offset   = offsetof (struct owl_pnuujw_vertex, position);

  vert_attr[1].binding  = 0;
  vert_attr[1].location = 1;
  vert_attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr[1].offset   = offsetof (struct owl_pnuujw_vertex, normal);

  vert_attr[2].binding  = 0;
  vert_attr[2].location = 2;
  vert_attr[2].format   = VK_FORMAT_R32G32_SFLOAT;
  vert_attr[2].offset   = offsetof (struct owl_pnuujw_vertex, uv0);

  vert_attr[3].binding  = 0;
  vert_attr[3].location = 3;
  vert_attr[3].format   = VK_FORMAT_R32G32_SFLOAT;
  vert_attr[3].offset   = offsetof (struct owl_pnuujw_vertex, uv1);

  vert_attr[4].binding  = 0;
  vert_attr[4].location = 4;
  vert_attr[4].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
  vert_attr[4].offset   = offsetof (struct owl_pnuujw_vertex, joints0);

  vert_attr[5].binding  = 0;
  vert_attr[5].location = 5;
  vert_attr[5].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
  vert_attr[5].offset   = offsetof (struct owl_pnuujw_vertex, weights0);

  vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input.pNext = NULL;
  vert_input.flags = 0;
  vert_input.vertexBindingDescriptionCount   = 1;
  vert_input.pVertexBindingDescriptions      = &vert_binding;
  vert_input.vertexAttributeDescriptionCount = owl_array_size (vert_attr);
  vert_input.pVertexAttributeDescriptions    = vert_attr;

  input_assembly.sType
      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext                  = NULL;
  input_assembly.flags                  = 0;
  input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x        = 0.0F;
  viewport.y        = 0.0F;
  viewport.width    = sc->extent.width;
  viewport.height   = sc->extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent   = sc->extent;

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &scissor;

  resterization.sType
      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  resterization.pNext                   = NULL;
  resterization.flags                   = 0;
  resterization.depthClampEnable        = VK_FALSE;
  resterization.rasterizerDiscardEnable = VK_FALSE;
  resterization.polygonMode             = VK_POLYGON_MODE_FILL;
  resterization.cullMode                = VK_CULL_MODE_BACK_BIT;
  resterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  resterization.depthBiasEnable         = VK_FALSE;
  resterization.depthBiasConstantFactor = 0.0F;
  resterization.depthBiasClamp          = 0.0F;
  resterization.depthBiasSlopeFactor    = 0.0F;
  resterization.lineWidth               = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = NULL;
  multisample.flags = 0;
  multisample.rasterizationSamples  = ctx->vk_msaa;
  multisample.sampleShadingEnable   = VK_FALSE;
  multisample.minSampleShading      = 1.0F;
  multisample.pSampleMask           = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable      = VK_FALSE;

  color_blend_attachment.blendEnable         = VK_FALSE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable     = VK_FALSE;
  color_blend.logicOp           = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount   = 1;
  color_blend.pAttachments      = &color_blend_attachment;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType
      = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext                 = NULL;
  depth_stencil.flags                 = 0;
  depth_stencil.depthTestEnable       = VK_TRUE;
  depth_stencil.depthWriteEnable      = VK_TRUE;
  depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
  owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext  = NULL;
  stages[0].flags  = 0;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = ps->vk_model_vert_shader;
  stages[0].pName  = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext  = NULL;
  stages[1].flags  = 0;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps->vk_model_frag_shader;
  stages[1].pName  = "main";
  stages[1].pSpecializationInfo = NULL;

  ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_MODEL]
      = ps->vk_model_pipeline_layout;

  info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext               = NULL;
  info.flags               = 0;
  info.stageCount          = owl_array_size (stages);
  info.pStages             = stages;
  info.pVertexInputState   = &vert_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState  = NULL;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &resterization;
  info.pMultisampleState   = &multisample;
  info.pDepthStencilState  = &depth_stencil;
  info.pColorBlendState    = &color_blend;
  info.pDynamicState       = NULL;
  info.layout              = ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_MODEL];
  info.renderPass          = ctx->vk_main_render_pass;
  info.subpass             = 0;
  info.basePipelineHandle  = VK_NULL_HANDLE;
  info.basePipelineIndex   = -1;

  /* TODO(samuel): create all pipelines in a single batch */
  vk_result = vkCreateGraphicsPipelines (
      ctx->vk_device, VK_NULL_HANDLE, 1, &info, NULL,
      &ps->vk_pipelines[OWL_VK_PIPELINE_ID_MODEL]);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_pipelines_model_deinit (struct owl_vk_pipelines     *ps,
                               struct owl_vk_context const *ctx)
{
  vkDestroyPipeline (ctx->vk_device,
                     ps->vk_pipelines[OWL_VK_PIPELINE_ID_MODEL], NULL);
}

owl_private enum owl_code
owl_vk_pipelines_skybox_init (struct owl_vk_pipelines       *ps,
                              struct owl_vk_context const   *ctx,
                              struct owl_vk_swapchain const *sc)
{
  VkVertexInputBindingDescription        vert_binding;
  VkVertexInputAttributeDescription      vert_attr;
  VkPipelineVertexInputStateCreateInfo   vert_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport                             viewport;
  VkRect2D                               scissor;
  VkPipelineViewportStateCreateInfo      viewport_state;
  VkPipelineRasterizationStateCreateInfo resterization;
  VkPipelineMultisampleStateCreateInfo   multisample;
  VkPipelineColorBlendAttachmentState    color_blend_attachment;
  VkPipelineColorBlendStateCreateInfo    color_blend;
  VkPipelineDepthStencilStateCreateInfo  depth_stencil;
  VkPipelineShaderStageCreateInfo        stages[2];
  VkGraphicsPipelineCreateInfo           info;
  VkResult                               vk_result;

  vert_binding.binding   = 0;
  vert_binding.stride    = sizeof (struct owl_p_vertex);
  vert_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  vert_attr.binding  = 0;
  vert_attr.location = 0;
  vert_attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
  vert_attr.offset   = offsetof (struct owl_p_vertex, position);

  vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vert_input.pNext = NULL;
  vert_input.flags = 0;
  vert_input.vertexBindingDescriptionCount   = 1;
  vert_input.pVertexBindingDescriptions      = &vert_binding;
  vert_input.vertexAttributeDescriptionCount = 1;
  vert_input.pVertexAttributeDescriptions    = &vert_attr;

  input_assembly.sType
      = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext                  = NULL;
  input_assembly.flags                  = 0;
  input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x        = 0.0F;
  viewport.y        = 0.0F;
  viewport.width    = sc->extent.width;
  viewport.height   = sc->extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent   = sc->extent;

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports    = &viewport;
  viewport_state.scissorCount  = 1;
  viewport_state.pScissors     = &scissor;

  resterization.sType
      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  resterization.pNext                   = NULL;
  resterization.flags                   = 0;
  resterization.depthClampEnable        = VK_FALSE;
  resterization.rasterizerDiscardEnable = VK_FALSE;
  resterization.polygonMode             = VK_POLYGON_MODE_FILL;
  resterization.cullMode                = VK_CULL_MODE_BACK_BIT;
  resterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  resterization.depthBiasEnable         = VK_FALSE;
  resterization.depthBiasConstantFactor = 0.0F;
  resterization.depthBiasClamp          = 0.0F;
  resterization.depthBiasSlopeFactor    = 0.0F;
  resterization.lineWidth               = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.pNext = NULL;
  multisample.flags = 0;
  multisample.rasterizationSamples  = ctx->vk_msaa;
  multisample.sampleShadingEnable   = VK_FALSE;
  multisample.minSampleShading      = 1.0F;
  multisample.pSampleMask           = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable      = VK_FALSE;

  color_blend_attachment.blendEnable         = VK_FALSE;
  color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
  color_blend_attachment.colorWriteMask
      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable     = VK_FALSE;
  color_blend.logicOp           = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount   = 1;
  color_blend.pAttachments      = &color_blend_attachment;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType
      = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext                 = NULL;
  depth_stencil.flags                 = 0;
  depth_stencil.depthTestEnable       = VK_FALSE;
  depth_stencil.depthWriteEnable      = VK_FALSE;
  depth_stencil.depthCompareOp        = VK_COMPARE_OP_NEVER;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable     = VK_FALSE;
  owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
  owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext  = NULL;
  stages[0].flags  = 0;
  stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = ps->vk_skybox_vert_shader;
  stages[0].pName  = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext  = NULL;
  stages[1].flags  = 0;
  stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = ps->vk_skybox_frag_shader;
  stages[1].pName  = "main";
  stages[1].pSpecializationInfo = NULL;

  ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_SKYBOX]
      = ps->vk_common_pipeline_layout;

  info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext               = NULL;
  info.flags               = 0;
  info.stageCount          = owl_array_size (stages);
  info.pStages             = stages;
  info.pVertexInputState   = &vert_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState  = NULL;
  info.pViewportState      = &viewport_state;
  info.pRasterizationState = &resterization;
  info.pMultisampleState   = &multisample;
  info.pDepthStencilState  = &depth_stencil;
  info.pColorBlendState    = &color_blend;
  info.pDynamicState       = NULL;
  info.layout             = ps->vk_pipeline_layouts[OWL_VK_PIPELINE_ID_SKYBOX];
  info.renderPass         = ctx->vk_main_render_pass;
  info.subpass            = 0;
  info.basePipelineHandle = VK_NULL_HANDLE;
  info.basePipelineIndex  = -1;

  /* TODO(samuel): create all pipelines in a single batch */
  vk_result = vkCreateGraphicsPipelines (
      ctx->vk_device, VK_NULL_HANDLE, 1, &info, NULL,
      &ps->vk_pipelines[OWL_VK_PIPELINE_ID_SKYBOX]);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_pipelines_skybox_deinit (struct owl_vk_pipelines     *ps,
                                struct owl_vk_context const *ctx)
{
  vkDestroyPipeline (ctx->vk_device,
                     ps->vk_pipelines[OWL_VK_PIPELINE_ID_SKYBOX], NULL);
}

owl_public enum owl_code
owl_vk_pipelines_init (struct owl_vk_pipelines *ps,
                       struct owl_vk_context   *ctx,
                       struct owl_vk_swapchain *sc)
{
  enum owl_code code;

  code = owl_vk_pipelines_layouts_init (ps, ctx);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_pipelines_shaders_init (ps, ctx);
  if (OWL_SUCCESS != code)
    goto error_pipeline_layouts_deinit;

  code = owl_vk_pipelines_basic_init (ps, ctx, sc);
  if (OWL_SUCCESS != code)
    goto error_shaders_deinit;

  code = owl_vk_pipelines_wires_init (ps, ctx, sc);
  if (OWL_SUCCESS != code)
    goto error_basic_deinit;

  code = owl_vk_pipelines_text_init (ps, ctx, sc);
  if (OWL_SUCCESS != code)
    goto error_wires_deinit;

  code = owl_vk_pipelines_model_init (ps, ctx, sc);
  if (OWL_SUCCESS != code)
    goto error_text_deinit;

  code = owl_vk_pipelines_skybox_init (ps, ctx, sc);
  if (OWL_SUCCESS != code)
    goto error_model_deinit;

  goto out;

error_model_deinit:
  owl_vk_pipelines_model_deinit (ps, ctx);

error_text_deinit:
  owl_vk_pipelines_text_deinit (ps, ctx);

error_wires_deinit:
  owl_vk_pipelines_wires_deinit (ps, ctx);

error_basic_deinit:
  owl_vk_pipelines_basic_deinit (ps, ctx);

error_shaders_deinit:
  owl_vk_pipelines_shaders_deinit (ps, ctx);

error_pipeline_layouts_deinit:
  owl_vk_pipelines_layouts_deinit (ps, ctx);

out:
  return code;
}

owl_public void
owl_vk_pipelines_deinit (struct owl_vk_pipelines *ps,
                         struct owl_vk_context   *ctx)
{
  owl_vk_pipelines_skybox_deinit (ps, ctx);
  owl_vk_pipelines_model_deinit (ps, ctx);
  owl_vk_pipelines_text_deinit (ps, ctx);
  owl_vk_pipelines_wires_deinit (ps, ctx);
  owl_vk_pipelines_basic_deinit (ps, ctx);
  owl_vk_pipelines_shaders_deinit (ps, ctx);
  owl_vk_pipelines_layouts_deinit (ps, ctx);
}

owl_public enum owl_code
owl_vk_pipelines_bind (struct owl_vk_pipelines   *ps,
                       enum owl_vk_pipeline_id    id,
                       struct owl_vk_frame const *frame)
{
  if (ps->active == id)
    return OWL_SUCCESS;

  ps->active = id;

  if (OWL_VK_PIPELINE_ID_NONE == id)
    return OWL_SUCCESS;

  vkCmdBindPipeline (frame->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     ps->vk_pipelines[id]);

  return OWL_SUCCESS;
}

owl_public VkPipelineLayout
owl_vk_pipelines_get_layout (struct owl_vk_pipelines const *ps)
{
  return ps->vk_pipeline_layouts[ps->active];
}

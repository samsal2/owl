#include "owl_vk_pipeline_manager.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"
#include "owl_vk_swapchain.h"
#include "owl_vk_types.h"

owl_private enum owl_code
owl_vk_pipeline_manager_pipeline_layouts_init (
    struct owl_vk_pipeline_manager *pm, struct owl_vk_context *ctx) {
  VkDescriptorSetLayout sets[6];
  VkPushConstantRange range;
  VkPipelineLayoutCreateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  sets[0] = ctx->vk_vert_ubo_set_layout;
  sets[1] = ctx->vk_frag_image_set_layout;

  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.setLayoutCount = 2;
  info.pSetLayouts = sets;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = NULL;

  vk_result = vkCreatePipelineLayout (ctx->vk_device, &info, NULL,
                                      &pm->vk_common_pipeline_layout);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  range.offset = 0;
  range.size = sizeof (struct owl_model_material_push_constant);

  sets[0] = ctx->vk_both_ubo_set_layout;
  sets[1] = ctx->vk_frag_image_set_layout;
  sets[2] = ctx->vk_frag_image_set_layout;
  sets[3] = ctx->vk_frag_image_set_layout;
  sets[4] = ctx->vk_vert_ssbo_set_layout;
  sets[5] = ctx->vk_frag_ubo_set_layout;

  info.pushConstantRangeCount = 1;
  info.pPushConstantRanges = &range;

  info.setLayoutCount = 6;

  vk_result = vkCreatePipelineLayout (ctx->vk_device, &info, NULL,
                                      &pm->vk_model_pipeline_layout);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_model_pipeline_layout_deinit;
  }

  goto out;

out_error_model_pipeline_layout_deinit:
  vkDestroyPipelineLayout (ctx->vk_device, pm->vk_common_pipeline_layout,
                           NULL);

out:

  return code;
}

owl_private void
owl_vk_pipeline_manager_pipeline_layouts_deinit (
    struct owl_vk_pipeline_manager *pm, struct owl_vk_context *ctx) {
  vkDestroyPipelineLayout (ctx->vk_device, pm->vk_model_pipeline_layout, NULL);
  vkDestroyPipelineLayout (ctx->vk_device, pm->vk_common_pipeline_layout,
                           NULL);
}

owl_private enum owl_code
owl_vk_pipeline_manager_shaders_init (struct owl_vk_pipeline_manager *pm,
                                      struct owl_vk_context *ctx) {
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo info;

    /* TODO(samuel): Properly load code at runtime */
    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result =
        vkCreateShaderModule (ctx->vk_device, &info, NULL, &pm->vk_basic_vert);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result =
        vkCreateShaderModule (ctx->vk_device, &info, NULL, &pm->vk_basic_frag);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_basic_vert_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_font.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result =
        vkCreateShaderModule (ctx->vk_device, &info, NULL, &pm->vk_text_frag);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_basic_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result =
        vkCreateShaderModule (ctx->vk_device, &info, NULL, &pm->vk_model_vert);

    if (VK_SUCCESS != vk_result) {
      goto out_error_text_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result =
        vkCreateShaderModule (ctx->vk_device, &info, NULL, &pm->vk_model_frag);

    if (VK_SUCCESS != vk_result) {
      goto out_error_model_vert_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &pm->vk_skybox_vert);

    if (VK_SUCCESS != vk_result) {
      goto out_error_model_frag_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (ctx->vk_device, &info, NULL,
                                      &pm->vk_skybox_frag);

    if (VK_SUCCESS != vk_result) {
      goto out_error_skybox_vert_shader_deinit;
    }
  }

  goto out;

out_error_skybox_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_skybox_vert, NULL);

out_error_model_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_model_frag, NULL);

out_error_model_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_model_vert, NULL);

out_error_text_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_text_frag, NULL);

out_error_basic_frag_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_basic_frag, NULL);

out_error_basic_vert_shader_deinit:
  vkDestroyShaderModule (ctx->vk_device, pm->vk_basic_vert, NULL);

out:
  return code;
}

owl_private void
owl_vk_pipeline_manager_shaders_deinit (struct owl_vk_pipeline_manager *pm,
                                        struct owl_vk_context *ctx) {
  vkDestroyShaderModule (ctx->vk_device, pm->vk_skybox_frag, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_skybox_vert, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_model_frag, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_model_vert, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_text_frag, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_basic_frag, NULL);
  vkDestroyShaderModule (ctx->vk_device, pm->vk_basic_vert, NULL);
}

owl_private enum owl_code
owl_vk_pipeline_manager_pipelines_init (struct owl_vk_pipeline_manager *pm,
                                        struct owl_vk_context *ctx,
                                        struct owl_vk_swapchain *swapchain) {
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_PIPELINE_ID_COUNT; ++i) {
    VkVertexInputBindingDescription vert_bindings[8];
    VkVertexInputAttributeDescription vert_attributes[8];
    VkPipelineVertexInputStateCreateInfo vert_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo resterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_blend_attachments[8];
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo info;

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
      vert_bindings[0].binding = 0;
      vert_bindings[0].stride = sizeof (struct owl_pcu_vertex);
      vert_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vert_attributes[0].binding = 0;
      vert_attributes[0].location = 0;
      vert_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vert_attributes[0].offset = offsetof (struct owl_pcu_vertex, position);

      vert_attributes[1].binding = 0;
      vert_attributes[1].location = 1;
      vert_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vert_attributes[1].offset = offsetof (struct owl_pcu_vertex, color);

      vert_attributes[2].binding = 0;
      vert_attributes[2].location = 2;
      vert_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
      vert_attributes[2].offset = offsetof (struct owl_pcu_vertex, uv);
      break;

    case OWL_PIPELINE_ID_MODEL:
      vert_bindings[0].binding = 0;
      vert_bindings[0].stride = sizeof (struct owl_pnuujw_vertex);
      vert_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vert_attributes[0].binding = 0;
      vert_attributes[0].location = 0;
      vert_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vert_attributes[0].offset =
          offsetof (struct owl_pnuujw_vertex, position);

      vert_attributes[1].binding = 0;
      vert_attributes[1].location = 1;
      vert_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vert_attributes[1].offset = offsetof (struct owl_pnuujw_vertex, normal);

      vert_attributes[2].binding = 0;
      vert_attributes[2].location = 2;
      vert_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
      vert_attributes[2].offset = offsetof (struct owl_pnuujw_vertex, uv0);

      vert_attributes[3].binding = 0;
      vert_attributes[3].location = 3;
      vert_attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
      vert_attributes[3].offset = offsetof (struct owl_pnuujw_vertex, uv1);

      vert_attributes[4].binding = 0;
      vert_attributes[4].location = 4;
      vert_attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vert_attributes[4].offset = offsetof (struct owl_pnuujw_vertex, joints0);

      vert_attributes[5].binding = 0;
      vert_attributes[5].location = 5;
      vert_attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vert_attributes[5].offset =
          offsetof (struct owl_pnuujw_vertex, weights0);
      break;
    case OWL_PIPELINE_ID_SKYBOX:
      vert_bindings[0].binding = 0;
      vert_bindings[0].stride = sizeof (struct owl_p_vertex);
      vert_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vert_attributes[0].binding = 0;
      vert_attributes[0].location = 0;
      vert_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vert_attributes[0].offset = offsetof (struct owl_p_vertex, position);
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
      vert_input.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vert_input.pNext = NULL;
      vert_input.flags = 0;
      vert_input.vertexBindingDescriptionCount = 1;
      vert_input.pVertexBindingDescriptions = vert_bindings;
      vert_input.vertexAttributeDescriptionCount = 3;
      vert_input.pVertexAttributeDescriptions = vert_attributes;
      break;

    case OWL_PIPELINE_ID_MODEL:
      vert_input.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vert_input.pNext = NULL;
      vert_input.flags = 0;
      vert_input.vertexBindingDescriptionCount = 1;
      vert_input.pVertexBindingDescriptions = vert_bindings;
      vert_input.vertexAttributeDescriptionCount = 6;
      vert_input.pVertexAttributeDescriptions = vert_attributes;
      break;
    case OWL_PIPELINE_ID_SKYBOX:
      vert_input.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vert_input.pNext = NULL;
      vert_input.flags = 0;
      vert_input.vertexBindingDescriptionCount = 1;
      vert_input.pVertexBindingDescriptions = vert_bindings;
      vert_input.vertexAttributeDescriptionCount = 1;
      vert_input.pVertexAttributeDescriptions = vert_attributes;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      input_assembly.sType =
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      input_assembly.pNext = NULL;
      input_assembly.flags = 0;
      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      input_assembly.primitiveRestartEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      viewport.x = 0.0F;
      viewport.y = 0.0F;
      viewport.width = swapchain->size.width;
      viewport.height = swapchain->size.height;
      viewport.minDepth = 0.0F;
      viewport.maxDepth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent = swapchain->size;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      viewport_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewport_state.pNext = NULL;
      viewport_state.flags = 0;
      viewport_state.viewportCount = 1;
      viewport_state.pViewports = &viewport;
      viewport_state.scissorCount = 1;
      viewport_state.pScissors = &scissor;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_SKYBOX:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_FILL;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;

    case OWL_PIPELINE_ID_MODEL:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_FILL;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;

    case OWL_PIPELINE_ID_WIRES:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_LINE;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      multisample.sType =
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisample.pNext = NULL;
      multisample.flags = 0;
      multisample.rasterizationSamples = ctx->msaa;
      multisample.sampleShadingEnable = VK_FALSE;
      multisample.minSampleShading = 1.0F;
      multisample.pSampleMask = NULL;
      multisample.alphaToCoverageEnable = VK_FALSE;
      multisample.alphaToOneEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      color_blend_attachments[0].blendEnable = VK_FALSE;
      color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;

    case OWL_PIPELINE_ID_TEXT:
      color_blend_attachments[0].blendEnable = VK_TRUE;
      color_blend_attachments[0].srcColorBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachments[0].dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].srcAlphaBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachments[0].dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
      color_blend.sType =
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      color_blend.pNext = NULL;
      color_blend.flags = 0;
      color_blend.logicOpEnable = VK_FALSE;
      color_blend.logicOp = VK_LOGIC_OP_COPY;
      color_blend.attachmentCount = 1;
      color_blend.pAttachments = color_blend_attachments;
      color_blend.blendConstants[0] = 0.0F;
      color_blend.blendConstants[1] = 0.0F;
      color_blend.blendConstants[2] = 0.0F;
      color_blend.blendConstants[3] = 0.0F;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
      depth_stencil.sType =
          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depth_stencil.pNext = NULL;
      depth_stencil.flags = 0;
      depth_stencil.depthTestEnable = VK_TRUE;
      depth_stencil.depthWriteEnable = VK_TRUE;
      depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
      depth_stencil.depthBoundsTestEnable = VK_FALSE;
      depth_stencil.stencilTestEnable = VK_FALSE;
      owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
      owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
      depth_stencil.minDepthBounds = 0.0F;
      depth_stencil.maxDepthBounds = 1.0F;
      break;
    case OWL_PIPELINE_ID_SKYBOX:
      depth_stencil.sType =
          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depth_stencil.pNext = NULL;
      depth_stencil.flags = 0;
      depth_stencil.depthTestEnable = VK_FALSE;
      depth_stencil.depthWriteEnable = VK_FALSE;
      depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
      depth_stencil.depthBoundsTestEnable = VK_FALSE;
      depth_stencil.stencilTestEnable = VK_FALSE;
      owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
      owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
      depth_stencil.minDepthBounds = 0.0F;
      depth_stencil.maxDepthBounds = 1.0F;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = pm->vk_basic_vert;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = pm->vk_basic_frag;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_PIPELINE_ID_TEXT:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = pm->vk_basic_vert;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = pm->vk_text_frag;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_PIPELINE_ID_MODEL:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = pm->vk_model_vert;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = pm->vk_model_frag;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_PIPELINE_ID_SKYBOX:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = pm->vk_skybox_vert;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = pm->vk_skybox_frag;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_SKYBOX:
      pm->vk_pipeline_layouts[i] = pm->vk_common_pipeline_layout;
      break;

    case OWL_PIPELINE_ID_MODEL:
      pm->vk_pipeline_layouts[i] = pm->vk_model_pipeline_layout;
      break;
    }

    switch (i) {
    case OWL_PIPELINE_ID_BASIC:
    case OWL_PIPELINE_ID_WIRES:
    case OWL_PIPELINE_ID_TEXT:
    case OWL_PIPELINE_ID_MODEL:
    case OWL_PIPELINE_ID_SKYBOX:
      info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.stageCount = owl_array_size (stages);
      info.pStages = stages;
      info.pVertexInputState = &vert_input;
      info.pInputAssemblyState = &input_assembly;
      info.pTessellationState = NULL;
      info.pViewportState = &viewport_state;
      info.pRasterizationState = &resterization;
      info.pMultisampleState = &multisample;
      info.pDepthStencilState = &depth_stencil;
      info.pColorBlendState = &color_blend;
      info.pDynamicState = NULL;
      info.layout = pm->vk_pipeline_layouts[i];
      info.renderPass = ctx->vk_main_render_pass;
      info.subpass = 0;
      info.basePipelineHandle = VK_NULL_HANDLE;
      info.basePipelineIndex = -1;
      break;
    }

    vk_result = vkCreateGraphicsPipelines (ctx->vk_device, VK_NULL_HANDLE, 1,
                                           &info, NULL, &pm->vk_pipelines[i]);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_pipelines_deinit;
    }
  }

  goto out;

out_error_pipelines_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyPipeline (ctx->vk_device, pm->vk_pipelines[i], NULL);
  }

out:
  return code;
}

owl_private void
owl_vk_pipeline_manager_pipelines_deinit (struct owl_vk_pipeline_manager *pm,
                                          struct owl_vk_context *ctx) {
  owl_i32 i;
  for (i = 0; i < OWL_PIPELINE_ID_COUNT; ++i)
    vkDestroyPipeline (ctx->vk_device, pm->vk_pipelines[i], NULL);
}

owl_public enum owl_code
owl_vk_pipeline_manager_init (struct owl_vk_pipeline_manager *pm,
                              struct owl_vk_context *ctx,
                              struct owl_vk_swapchain *swapchain) {
  enum owl_code code;

  code = owl_vk_pipeline_manager_pipeline_layouts_init (pm, ctx);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_pipeline_manager_shaders_init (pm, ctx);
  if (OWL_SUCCESS != code)
    goto out_error_pipeline_layouts_deinit;

  code = owl_vk_pipeline_manager_pipelines_init (pm, ctx, swapchain);
  if (OWL_SUCCESS != code)
    goto out_error_shaders_deinit;

  goto out;

out_error_shaders_deinit:
  owl_vk_pipeline_manager_shaders_deinit (pm, ctx);

out_error_pipeline_layouts_deinit:
  owl_vk_pipeline_manager_pipeline_layouts_deinit (pm, ctx);

out:
  return code;
}

owl_public void
owl_vk_pipeline_manager_deinit (struct owl_vk_pipeline_manager *pm,
                                struct owl_vk_context *ctx) {
  owl_vk_pipeline_manager_pipelines_deinit (pm, ctx);
  owl_vk_pipeline_manager_shaders_deinit (pm, ctx);
  owl_vk_pipeline_manager_pipeline_layouts_deinit (pm, ctx);
}

owl_public enum owl_code
owl_vk_pipeline_manager_bind (struct owl_vk_pipeline_manager *pm,
                              enum owl_pipeline_id id,
                              struct owl_vk_frame const *frame) {
  if (pm->active_pipeline == id)
    return OWL_SUCCESS;

  pm->active_pipeline = id;

  if (OWL_PIPELINE_ID_NONE == id)
    return OWL_SUCCESS;

  vkCmdBindPipeline (frame->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                     pm->vk_pipelines[id]);

  return OWL_SUCCESS;
}

owl_public VkPipelineLayout
owl_vk_pipeline_manager_get_layout (struct owl_vk_pipeline_manager const *pm) {
  return pm->vk_pipeline_layouts[pm->active_pipeline];
}

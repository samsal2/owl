#include "owl_fluid_simulation.h"

#include "owl_internal.h"
#include "owl_vector_math.h"

#define OWL_SIM_RESOLUTION 128
#define OWL_DYE_RESOLUTION 1024

OWLAPI int
owl_fluid_simulation_buffer_init(struct owl_renderer *r, void const *data,
                                 uint32_t width, uint32_t height,
                                 struct owl_fluid_simulation_buffer *buffer) {
  int ret = OWL_OK;
  VkDevice const device = r->device;

  {
    struct owl_texture_desc desc;

    desc.type = OWL_TEXTURE_TYPE_COMPUTE;
    desc.source = OWL_TEXTURE_SOURCE_DATA;
    desc.path = NULL;
    desc.pixels = data;
    desc.width = width;
    desc.height = height;
    desc.format = OWL_RGBA32_SFLOAT;

    ret = owl_texture_init(r, &desc, &buffer->texture);
    if (ret)
      goto error;
  }

  {
    VkDescriptorSetAllocateInfo info;
    VkResult vk_result;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->descriptor_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &r->fluid_simulation_source_descriptor_set_layout;

    vk_result = vkAllocateDescriptorSets(device, &info,
                                         &buffer->storage_descriptor_set);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_deinit_texture;
    }
  }

  {
    VkDescriptorImageInfo descriptor;
    VkWriteDescriptorSet write;

    descriptor.sampler = NULL;
    descriptor.imageView = buffer->texture.image_view;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = buffer->storage_descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &descriptor;
    write.pBufferInfo = NULL;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
  }

  return OWL_OK;

error_deinit_texture:
  owl_texture_deinit(r, &buffer->texture);

error:
  return ret;
}

OWLAPI void owl_fluid_simulation_buffer_deinit(
    struct owl_renderer *r, struct owl_fluid_simulation_buffer *buffer) {
  VkDevice const device = r->device;

  vkDeviceWaitIdle(device);

  vkFreeDescriptorSets(device, r->descriptor_pool, 1,
                       &buffer->storage_descriptor_set);
  owl_texture_deinit(r, &buffer->texture);
}

OWLAPI int owl_fluid_simulation_init(struct owl_fluid_simulation *sim,
                                     struct owl_renderer *r) {
  int i;
  owl_v4 *dye;
  owl_v4 *vec;
  int ret = OWL_OK;
  VkDevice const device = r->device;

  sim->requires_record = 1;

  {
    VkBufferCreateInfo info;
    VkResult vk_result;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = sizeof(struct owl_fluid_simulation_uniform);
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(device, &info, NULL, &sim->uniform_buffer);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error;
    }
  }

  {
    void *data;
    VkMemoryPropertyFlagBits properties;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device, sim->uniform_buffer, &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL, &sim->uniform_memory);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_destroy_uniform_buffer;
    }

    vk_result = vkBindBufferMemory(device, sim->uniform_buffer,
                                   sim->uniform_memory, 0);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_free_uniform_memory;
    }

    vk_result = vkMapMemory(device, sim->uniform_memory, 0, VK_WHOLE_SIZE, 0,
                            &data);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_free_uniform_memory;
    }

    sim->uniform = data;
  }

  {
    VkDescriptorSetAllocateInfo info;
    VkResult vk_result;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->descriptor_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &r->fluid_simulation_uniform_descriptor_set_layout;

    vk_result = vkAllocateDescriptorSets(device, &info,
                                         &sim->uniform_descriptor_set);
    if (vk_result)
      goto error_free_uniform_memory;
  }

  {
    VkCommandPoolCreateInfo info;
    VkResult vk_result;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = r->compute_family;

    vk_result = vkCreateCommandPool(device, &info, NULL, &sim->command_pool);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_free_uniform_descriptor_sets;
    }
  }

  {
    VkCommandBufferAllocateInfo info;
    VkResult vk_result;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = sim->command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(device, &info, &sim->command_buffer);
    if (vk_result) {
      ret = OWL_ERROR_FATAL;
      goto error_destroy_command_pool;
    }
  }

  {
    VkDescriptorBufferInfo descriptor;
    VkWriteDescriptorSet write;

    descriptor.buffer = sim->uniform_buffer;
    descriptor.offset = 0;
    descriptor.range = sizeof(struct owl_fluid_simulation_uniform);

    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = NULL;
    write.dstSet = sim->uniform_descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pImageInfo = NULL;
    write.pBufferInfo = &descriptor;
    write.pTexelBufferView = NULL;

    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
  }

  /* NOTE(samuel): MAKE SURE IT's 0 INITIALIZED */
  dye = OWL_CALLOC(OWL_DYE_RESOLUTION * OWL_DYE_RESOLUTION, sizeof(*dye));
  if (!dye) {
    ret = OWL_ERROR_NO_MEMORY;
    goto error_free_command_buffer;
  }

  /* NOTE(samuel): MAKE SURE IT's 0 INITIALIZED */
  vec = OWL_CALLOC(OWL_SIM_RESOLUTION * OWL_SIM_RESOLUTION, sizeof(*vec));
  if (!vec) {
    ret = OWL_ERROR_NO_MEMORY;
    goto error_free_dye;
  }

  sim->velocity_read = 1;
  sim->velocity_write = 0;
  for (i = 0; i < 2; ++i) {
    struct owl_fluid_simulation_buffer *buffer = &sim->velocity[i];
    ret = owl_fluid_simulation_buffer_init(r, vec, OWL_SIM_RESOLUTION,
                                           OWL_SIM_RESOLUTION, buffer);
    if (ret)
      goto error_deinit_velocity;
  }

  sim->pressure_read = 1;
  sim->pressure_write = 0;
  for (i = 0; i < 2; ++i) {
    struct owl_fluid_simulation_buffer *buffer = &sim->pressure[i];
    ret = owl_fluid_simulation_buffer_init(r, vec, OWL_SIM_RESOLUTION,
                                           OWL_SIM_RESOLUTION, buffer);
    if (ret)
      goto error_deinit_pressure;
  }

  dye[1024 * 512 + 512][0] = 255;
  dye[1024 * 512 + 512][1] = 255;
  dye[1024 * 512 + 512][2] = 255;
  dye[1024 * 512 + 512][3] = 255;

  dye[1024 * 512 + 593][0] = 255;
  dye[1024 * 512 + 593][1] = 255;
  dye[1024 * 512 + 593][2] = 255;
  dye[1024 * 512 + 593][3] = 255;

  sim->dye_read = 1;
  sim->dye_write = 0;
  for (i = 0; i < 2; ++i) {
    struct owl_fluid_simulation_buffer *buffer = &sim->dye[i];
    ret = owl_fluid_simulation_buffer_init(r, dye, OWL_DYE_RESOLUTION,
                                           OWL_DYE_RESOLUTION, buffer);
    if (ret)
      if (ret)
        goto error_deinit_dye;
  }

  ret = owl_fluid_simulation_buffer_init(r, vec, OWL_SIM_RESOLUTION,
                                         OWL_SIM_RESOLUTION, &sim->curl);
  if (ret)
    goto error_deinit_dye;

  ret = owl_fluid_simulation_buffer_init(r, vec, OWL_SIM_RESOLUTION,
                                         OWL_SIM_RESOLUTION, &sim->divergence);
  if (ret)
    goto error_deinit_curl;

  OWL_FREE(vec);
  OWL_FREE(dye);

  return OWL_OK;

error_deinit_curl:
  owl_fluid_simulation_buffer_deinit(r, &sim->curl);

error_deinit_dye:
  for (i = i - 1; i >= 0; --i)
    owl_fluid_simulation_buffer_deinit(r, &sim->dye[i]);

  i = 2;

error_deinit_pressure:
  for (i = i - 1; i >= 0; --i)
    owl_fluid_simulation_buffer_deinit(r, &sim->pressure[i]);

  i = 2;

error_deinit_velocity:
  for (i = i - 1; i >= 0; --i)
    owl_fluid_simulation_buffer_deinit(r, &sim->velocity[i]);

  /* error_free_vec: */
  OWL_FREE(vec);

error_free_dye:
  OWL_FREE(dye);

error_free_command_buffer:
  vkFreeCommandBuffers(device, sim->command_pool, 1, &sim->command_buffer);

error_destroy_command_pool:
  vkDestroyCommandPool(device, sim->command_pool, NULL);

error_free_uniform_descriptor_sets:
  vkFreeDescriptorSets(device, r->descriptor_pool, 1,
                       &sim->uniform_descriptor_set);

error_free_uniform_memory:
  vkFreeMemory(device, sim->uniform_memory, NULL);

error_destroy_uniform_buffer:
  vkDestroyBuffer(device, sim->uniform_buffer, NULL);

error:
  return ret;
}

OWLAPI void owl_fluid_simulation_deinit(struct owl_fluid_simulation *sim,
                                        struct owl_renderer *r) {
  int i;

  VkDevice const device = r->device;

  owl_fluid_simulation_buffer_deinit(r, &sim->divergence);
  owl_fluid_simulation_buffer_deinit(r, &sim->curl);

  for (i = 0; i < (int)OWL_ARRAY_SIZE(sim->dye); ++i)
    owl_fluid_simulation_buffer_deinit(r, &sim->dye[i]);

  for (i = 0; i < (int)OWL_ARRAY_SIZE(sim->pressure); ++i)
    owl_fluid_simulation_buffer_deinit(r, &sim->pressure[i]);

  for (i = 0; i < (int)OWL_ARRAY_SIZE(sim->velocity); ++i)
    owl_fluid_simulation_buffer_deinit(r, &sim->velocity[i]);

  vkFreeCommandBuffers(device, sim->command_pool, 1, &sim->command_buffer);
  vkDestroyCommandPool(device, sim->command_pool, NULL);
  vkFreeDescriptorSets(device, r->descriptor_pool, 1,
                       &sim->uniform_descriptor_set);
  vkFreeMemory(device, sim->uniform_memory, NULL);
  vkDestroyBuffer(device, sim->uniform_buffer, NULL);
}

OWLAPI void owl_fluid_simulation_update(struct owl_fluid_simulation *sim,
                                        struct owl_renderer *r, float dt) {
  VkSubmitInfo submit_info;
  VkPipelineStageFlagBits stage;
  VkCommandBuffer const command_buffer = sim->command_buffer;

  sim->uniform->dt = dt;

  if (sim->requires_record) {
    VkDescriptorSet descriptor_sets[4];
    VkCommandBufferBeginInfo begin_info;
    struct owl_fluid_simulation_push_constant push_constant;

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = NULL;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    /* bind global uniform */
    sim->uniform->point[0] = 0;
    sim->uniform->point[1] = 0;

    sim->uniform->radius = 1.0F;
    sim->uniform->sim_texel_size[0] = 1.0F / (float)OWL_SIM_RESOLUTION;
    sim->uniform->sim_texel_size[1] = 1.0F / (float)OWL_SIM_RESOLUTION;
    sim->uniform->dye_texel_size[0] = 1.0F / (float)OWL_DYE_RESOLUTION;
    sim->uniform->dye_texel_size[1] = 1.0F / (float)OWL_DYE_RESOLUTION;

    sim->uniform->color[0] = 1.0F;
    sim->uniform->color[1] = 1.0F;
    sim->uniform->color[2] = 1.0F;

    sim->uniform->curl = 30.0F;

    descriptor_sets[0] = sim->uniform_descriptor_set;

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->fluid_simulation_pipeline_layout, 0, 1,
                            &descriptor_sets[0], 0, NULL);

    /* dispatch curl */
    descriptor_sets[1] =
        sim->velocity[sim->velocity_read].storage_descriptor_set;
    descriptor_sets[3] = sim->curl.storage_descriptor_set;

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->fluid_simulation_pipeline_layout, 1, 1,
                            &descriptor_sets[1], 0, NULL);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->fluid_simulation_pipeline_layout, 3, 1,
                            &descriptor_sets[3], 0, NULL);

    push_constant.texel_size[0] = 1.0F / (float)OWL_SIM_RESOLUTION;
    push_constant.texel_size[1] = 1.0F / (float)OWL_SIM_RESOLUTION;

    vkCmdPushConstants(command_buffer, r->fluid_simulation_pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constant),
                       &push_constant);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      r->fluid_simulation_curl_pipeline);

    vkCmdDispatch(command_buffer, OWL_SIM_RESOLUTION, OWL_SIM_RESOLUTION, 1);

    vkEndCommandBuffer(command_buffer);
  }

  stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = NULL;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;
  submit_info.pWaitDstStageMask = &stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vkQueueSubmit(r->compute_queue, 1, &submit_info, NULL);
  /* TODO(samuel): begin lazy here, just use a semaphore */
  vkQueueWaitIdle(r->compute_queue);
}

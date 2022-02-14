#include "draw_cmd.h"

#include "internal.h"
#include "renderer.h"
#include "skybox.h"
#include "texture.h"
#include "vector_math.h"

OWL_INTERNAL enum owl_code
owl_draw_cmd_submit_basic_(struct owl_vk_renderer *r,
                           struct owl_draw_cmd_basic const *basic) {
  enum owl_code code = OWL_SUCCESS;

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(*basic->vertices) * (VkDeviceSize)basic->vertices_count;
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->vertices, size);

    vkCmdBindVertexBuffers(r->frame_cmd_buffers[r->active], 0, 1, &ref.buffer,
                           &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(*basic->indices) * (VkDeviceSize)basic->indices_count;
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->indices, size);

    vkCmdBindIndexBuffer(r->frame_cmd_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(basic->ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, &basic->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = basic->texture->set;

    vkCmdBindDescriptorSets(r->frame_cmd_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  vkCmdDrawIndexed(r->frame_cmd_buffers[r->active],
                   (owl_u32)basic->indices_count, 1, 0, 0, 0);

  return code;
}

OWL_INTERNAL enum owl_code
owl_draw_cmd_submit_quad_(struct owl_vk_renderer *r,
                          struct owl_draw_cmd_quad const *quad) {
  enum owl_code code = OWL_SUCCESS;

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(quad->vertices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, quad->vertices, size);

    vkCmdBindVertexBuffers(r->frame_cmd_buffers[r->active], 0, 1, &ref.buffer,
                           &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;
    owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(r->frame_cmd_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(quad->ubo);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, &quad->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = quad->texture->set;

    vkCmdBindDescriptorSets(r->frame_cmd_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  vkCmdDrawIndexed(r->frame_cmd_buffers[r->active], 6, 1, 0, 0, 0);

  return code;
}

OWL_INTERNAL enum owl_code
owl_draw_cmd_submit_skybox_(struct owl_vk_renderer *r,
                            struct owl_draw_cmd_skybox const *skybox) {
  enum owl_code code = OWL_SUCCESS;

  OWL_LOCAL_PERSIST owl_v3 const vertices[] = {
#if 1
    {-1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, 1.0f},
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},

    {-1.0f, 1.0f, -1.0f},
    {-1.0f, 1.0f, 1.0f},
    {-1.0f, -1.0f, 1.0f},
#endif

#if 1
    {1.0f, -1.0f, -1.0f},
    {1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},

    {1.0f, 1.0f, -1.0f},
    {1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
#endif

#if 1
    {-1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},
    {1.0f, 1.0f, 1.0f},

    {1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, 1.0f},
    {-1.0f, 1.0f, -1.0f},
#endif

#if 1
    {-1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, -1.0f},

    {1.0f, -1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},
    {1.0f, -1.0f, 1.0f}
#endif
  };

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;

    size = sizeof(vertices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, vertices, size);

    vkCmdBindVertexBuffers(r->frame_cmd_buffers[r->active], 0, 1, &ref.buffer,
                           &ref.offset);
  }

#if 0
  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dynamic_buffer_reference ref;
    owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(r->frame_cmd_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }
#endif

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dynamic_buffer_reference ref;
    struct owl_skybox_uniform uniform;

    size = sizeof(struct owl_skybox_uniform);
    data = owl_renderer_dynamic_buffer_alloc(r, size, &ref);

#if 1
    OWL_COPY_M4(skybox->ubo.projection, uniform.projection);
#else
    OWL_IDENTITY_M4(uniform.projection);
#endif

#if 1
    OWL_IDENTITY_M4(uniform.view);
    OWL_COPY_M3(skybox->ubo.view, uniform.view);
#else
    OWL_IDENTITY_M4(uniform.view);
#endif

    OWL_MEMCPY(data, &uniform, size);

    sets[0] = ref.pvm_set;
    sets[1] = skybox->skybox->set;

    vkCmdBindDescriptorSets(r->frame_cmd_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

#if 0
  vkCmdDrawIndexed(r->frame_cmd_buffers[r->active], 6, 1, 0, 0, 0);
#else
  vkCmdDraw(r->frame_cmd_buffers[r->active], OWL_ARRAY_SIZE(vertices), 1, 0, 0);
#endif

  return code;
}

enum owl_code owl_draw_cmd_submit(struct owl_vk_renderer *r,
                                  struct owl_draw_cmd const *cmd) {
  switch (cmd->type) {
  case OWL_DRAW_CMD_TYPE_BASIC:
    return owl_draw_cmd_submit_basic_(r, &cmd->storage.as_basic);

  case OWL_DRAW_CMD_TYPE_QUAD:
    return owl_draw_cmd_submit_quad_(r, &cmd->storage.as_quad);

  case OWL_DRAW_CMD_TYPE_SKYBOX:
    return owl_draw_cmd_submit_skybox_(r, &cmd->storage.as_skybox);
  }
}

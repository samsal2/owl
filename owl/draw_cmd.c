#include "draw_cmd.h"

#include "internal.h"
#include "renderer.h"
#include "texture.h"

OWL_INTERNAL enum owl_code
owl_submit_draw_cmd_basic_(struct owl_vk_renderer *r,
                           struct owl_draw_cmd_basic const *basic) {
  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dyn_buffer_ref ref;

    size = sizeof(*basic->vertices) * (VkDeviceSize)basic->vertices_count;
    data = owl_dyn_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->vertices, size);

    vkCmdBindVertexBuffers(r->frame_cmd_buffers[r->active], 0, 1, &ref.buffer,
                           &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dyn_buffer_ref ref;

    size = sizeof(*basic->indices) * (VkDeviceSize)basic->indices_count;
    data = owl_dyn_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, basic->indices, size);

    vkCmdBindIndexBuffer(r->frame_cmd_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dyn_buffer_ref ref;

    size = sizeof(basic->ubo);
    data = owl_dyn_buffer_alloc(r, size, &ref);

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

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code
owl_submit_draw_cmd_quad_(struct owl_vk_renderer *r,
                          struct owl_draw_cmd_quad const *quad) {
  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dyn_buffer_ref ref;

    size = sizeof(quad->vertices);
    data = owl_dyn_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, quad->vertices, size);

    vkCmdBindVertexBuffers(r->frame_cmd_buffers[r->active], 0, 1, &ref.buffer,
                           &ref.offset);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    struct owl_dyn_buffer_ref ref;
    owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_dyn_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(r->frame_cmd_buffers[r->active], ref.buffer,
                         ref.offset, VK_INDEX_TYPE_UINT32);
  }

  {
    owl_byte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dyn_buffer_ref ref;

    size = sizeof(quad->ubo);
    data = owl_dyn_buffer_alloc(r, size, &ref);

    OWL_MEMCPY(data, &quad->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = quad->texture->set;

    vkCmdBindDescriptorSets(r->frame_cmd_buffers[r->active],
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            r->pipeline_layouts[r->bound_pipeline], 0,
                            OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  vkCmdDrawIndexed(r->frame_cmd_buffers[r->active], 6, 1, 0, 0, 0);

  return OWL_SUCCESS;
}

enum owl_code owl_submit_draw_cmd(struct owl_vk_renderer *renderer,
                                  struct owl_draw_cmd const *cmd) {
  switch (cmd->type) {
  case OWL_DRAW_CMD_TYPE_BASIC:
    return owl_submit_draw_cmd_basic_(renderer, &cmd->storage.as_basic);

  case OWL_DRAW_CMD_TYPE_QUAD:
    return owl_submit_draw_cmd_quad_(renderer, &cmd->storage.as_quad);
  }
}

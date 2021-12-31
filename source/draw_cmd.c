#include "../include/owl/draw_cmd.h"

#include "dyn_buf.h"
#include "img.h"
#include "internal.h"
#include "vk_renderer.h"

OWL_INTERNAL enum owl_code
owl_submit_draw_cmd_basic_(struct owl_vk_renderer *renderer,
                           struct owl_draw_cmd_basic const *basic) {
  OwlU32 const active = renderer->dyn_active_buf;

  {
    OwlByte *data;
    VkDeviceSize size;
    struct owl_dyn_buf_ref ref;

    size = sizeof(*basic->vertices) * (unsigned)basic->vertex_count;
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, basic->vertices, size);

    vkCmdBindVertexBuffers(renderer->frame_cmd_bufs[active], 0, 1, &ref.buf,
                           &ref.offset);
  }

  {
    OwlByte *data;
    VkDeviceSize size;
    struct owl_dyn_buf_ref ref;

    size = sizeof(*basic->indices) * (unsigned)basic->index_count;
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, basic->indices, size);

    vkCmdBindIndexBuffer(renderer->frame_cmd_bufs[active], ref.buf, ref.offset,
                         VK_INDEX_TYPE_UINT32);
  }

  {
    OwlByte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dyn_buf_ref ref;

    size = sizeof(basic->ubo);
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, &basic->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = basic->img->set;

    vkCmdBindDescriptorSets(
        renderer->frame_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderer->pipeline_layouts[renderer->bound_pipeline], 0,
        OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  vkCmdDrawIndexed(renderer->frame_cmd_bufs[active],
                   (unsigned)basic->index_count, 1, 0, 0, 0);

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code
owl_submit_draw_cmd_quad_(struct owl_vk_renderer *renderer,
                          struct owl_draw_cmd_quad const *quad) {
  OwlU32 const active = renderer->dyn_active_buf;

  {
    OwlByte *data;
    VkDeviceSize size;
    struct owl_dyn_buf_ref ref;

    size = sizeof(quad->vertices);
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, quad->vertices, size);

    vkCmdBindVertexBuffers(renderer->frame_cmd_bufs[active], 0, 1, &ref.buf,
                           &ref.offset);
  }

  {
    OwlByte *data;
    VkDeviceSize size;
    struct owl_dyn_buf_ref ref;
    OwlU32 const indices[] = {2, 3, 1, 1, 0, 2};

    size = sizeof(indices);
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, indices, size);

    vkCmdBindIndexBuffer(renderer->frame_cmd_bufs[active], ref.buf, ref.offset,
                         VK_INDEX_TYPE_UINT32);
  }

  {
    OwlByte *data;
    VkDeviceSize size;
    VkDescriptorSet sets[2];
    struct owl_dyn_buf_ref ref;

    size = sizeof(quad->ubo);
    data = owl_dyn_buf_alloc(renderer, size, &ref);

    OWL_MEMCPY(data, &quad->ubo, size);

    sets[0] = ref.pvm_set;
    sets[1] = quad->img->set;

    vkCmdBindDescriptorSets(
        renderer->frame_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
        renderer->pipeline_layouts[renderer->bound_pipeline], 0,
        OWL_ARRAY_SIZE(sets), sets, 1, &ref.offset32);
  }

  vkCmdDrawIndexed(renderer->frame_cmd_bufs[active], 6, 1, 0, 0, 0);

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

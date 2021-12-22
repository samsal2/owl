#include <owl/internal.h>
#include <owl/memory.h>
#include <owl/render_group.h>
#include <owl/renderer_internal.h>
#include <owl/texture_internal.h>

OWL_INTERNAL enum owl_code
owl_submit_render_group_basic_(struct owl_renderer *renderer,
                               struct owl_render_group_basic const *basic) {
  OwlByte *data;
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_tmp_submit_mem_ref vtx;
  struct owl_tmp_submit_mem_ref idx;
  struct owl_tmp_submit_mem_ref pvm;
  OwlU32 const active = renderer->active_buf;

  size = sizeof(*basic->vertex) * (unsigned)basic->vertex_count;
  data = owl_alloc_tmp_submit_mem(renderer, size, &vtx);

  OWL_MEMCPY(data, basic->vertex, size);

  size = sizeof(*basic->index) * (unsigned)basic->index_count;
  data = owl_alloc_tmp_submit_mem(renderer, size, &idx);

  OWL_MEMCPY(data, basic->index, size);

  size = sizeof(basic->pvm);
  data = owl_alloc_tmp_submit_mem(renderer, size, &pvm);

  OWL_MEMCPY(data, &basic->pvm, size);

  sets[0] = pvm.set;
  sets[1] = owl_get_texture(basic->texture)->set;

  vkCmdBindVertexBuffers(renderer->draw_cmd_bufs[active], 0, 1, &vtx.buf,
                         &vtx.offset);

  vkCmdBindIndexBuffer(renderer->draw_cmd_bufs[active], idx.buf, idx.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->draw_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->pipeline_layouts[renderer->bound_pipeline], 0,
      OWL_ARRAY_SIZE(sets), sets, 1, &pvm.offset32);

  vkCmdDrawIndexed(renderer->draw_cmd_bufs[active],
                   (unsigned)basic->index_count, 1, 0, 0, 0);

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code
owl_submit_render_group_quad_(struct owl_renderer *renderer,
                              struct owl_render_group_quad const *quad) {
  OwlByte *data;
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_tmp_submit_mem_ref vtx;
  struct owl_tmp_submit_mem_ref idx;
  struct owl_tmp_submit_mem_ref pvm;
  OwlU32 const indices[] = {2, 3, 1, 1, 0, 2};
  OwlU32 const active = renderer->active_buf;

  size = sizeof(quad->vertex);
  data = owl_alloc_tmp_submit_mem(renderer, size, &vtx);

  OWL_MEMCPY(data, quad->vertex, size);

  size = sizeof(indices);
  data = owl_alloc_tmp_submit_mem(renderer, size, &idx);

  OWL_MEMCPY(data, indices, size);

  size = sizeof(quad->pvm);
  data = owl_alloc_tmp_submit_mem(renderer, size, &pvm);

  OWL_MEMCPY(data, &quad->pvm, size);

  sets[0] = pvm.set;
  sets[1] = owl_get_texture(quad->texture)->set;

  vkCmdBindVertexBuffers(renderer->draw_cmd_bufs[active], 0, 1, &vtx.buf,
                         &vtx.offset);

  vkCmdBindIndexBuffer(renderer->draw_cmd_bufs[active], idx.buf, idx.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->draw_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->pipeline_layouts[renderer->bound_pipeline], 0,
      OWL_ARRAY_SIZE(sets), sets, 1, &pvm.offset32);

  vkCmdDrawIndexed(renderer->draw_cmd_bufs[active], OWL_ARRAY_SIZE(indices),
                   1, 0, 0, 0);

  return OWL_SUCCESS;
}

enum owl_code owl_submit_render_group(struct owl_renderer *renderer,
                                      struct owl_render_group const *group) {
  switch (group->type) {
  case OWL_RENDER_GROUP_TYPE_NOP:
    return OWL_SUCCESS;

  case OWL_RENDER_GROUP_TYPE_BASIC:
    return owl_submit_render_group_basic_(renderer, &group->storage.as_basic);

  case OWL_RENDER_GROUP_TYPE_QUAD:
    return owl_submit_render_group_quad_(renderer, &group->storage.as_quad);
  }
}

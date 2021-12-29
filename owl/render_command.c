#include <owl/internal.h>
#include <owl/memory.h>
#include <owl/render_command.h>
#include <owl/texture.h>
#include <owl/vk_renderer.h>
#include <owl/vk_texture_manager.h>

OWL_INTERNAL enum owl_code
owl_submit_render_group_basic_(struct owl_vk_renderer *renderer,
                               struct owl_render_command_basic const *basic) {
  OwlByte *data;
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_tmp_submit_mem_ref vtx;
  struct owl_tmp_submit_mem_ref idx;
  struct owl_tmp_submit_mem_ref pvm;
  OwlU32 const active = renderer->dyn_active_buf;
  struct owl_vk_texture_manager *tmgr = owl_peek_texture_manager_(renderer);

  size = sizeof(*basic->vertex) * (unsigned)basic->vertex_count;
  data = owl_alloc_tmp_submit_mem(renderer, size, &vtx);

  OWL_MEMCPY(data, basic->vertex, size);

  size = sizeof(*basic->index) * (unsigned)basic->index_count;
  data = owl_alloc_tmp_submit_mem(renderer, size, &idx);

  OWL_MEMCPY(data, basic->index, size);

  size = sizeof(basic->pvm);
  data = owl_alloc_tmp_submit_mem(renderer, size, &pvm);

  OWL_MEMCPY(data, &basic->pvm, size);

  sets[0] = pvm.pvm_set;
  sets[1] = tmgr->textures[basic->texture].set;

  vkCmdBindVertexBuffers(renderer->frame_cmd_bufs[active], 0, 1, &vtx.buf,
                         &vtx.offset);

  vkCmdBindIndexBuffer(renderer->frame_cmd_bufs[active], idx.buf, idx.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->frame_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->pipeline_layouts[renderer->bound_pipeline], 0,
      OWL_ARRAY_SIZE(sets), sets, 1, &pvm.offset32);

  vkCmdDrawIndexed(renderer->frame_cmd_bufs[active],
                   (unsigned)basic->index_count, 1, 0, 0, 0);

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code
owl_submit_render_group_quad_(struct owl_vk_renderer *renderer,
                              struct owl_render_command_quad const *quad) {
  OwlByte *data;
  VkDeviceSize size;
  VkDescriptorSet sets[2];
  struct owl_tmp_submit_mem_ref vtx;
  struct owl_tmp_submit_mem_ref idx;
  struct owl_tmp_submit_mem_ref pvm;
  OwlU32 const indices[] = {2, 3, 1, 1, 0, 2};
  OwlU32 const active = renderer->dyn_active_buf;
  struct owl_vk_texture_manager *tmgr = owl_peek_texture_manager_(renderer);

  size = sizeof(quad->vertex);
  data = owl_alloc_tmp_submit_mem(renderer, size, &vtx);

  OWL_MEMCPY(data, quad->vertex, size);

  size = sizeof(indices);
  data = owl_alloc_tmp_submit_mem(renderer, size, &idx);

  OWL_MEMCPY(data, indices, size);

  size = sizeof(quad->pvm);
  data = owl_alloc_tmp_submit_mem(renderer, size, &pvm);

  OWL_MEMCPY(data, &quad->pvm, size);

  sets[0] = pvm.pvm_set;
  sets[1] = tmgr->textures[quad->texture].set;

  vkCmdBindVertexBuffers(renderer->frame_cmd_bufs[active], 0, 1, &vtx.buf,
                         &vtx.offset);

  vkCmdBindIndexBuffer(renderer->frame_cmd_bufs[active], idx.buf, idx.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->frame_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->pipeline_layouts[renderer->bound_pipeline], 0,
      OWL_ARRAY_SIZE(sets), sets, 1, &pvm.offset32);

  vkCmdDrawIndexed(renderer->frame_cmd_bufs[active], OWL_ARRAY_SIZE(indices),
                   1, 0, 0, 0);

  return OWL_SUCCESS;
}

#if 0
OWL_INTERNAL enum owl_code owl_submit_render_group_light_(
    struct owl_vk_renderer *renderer,
    struct owl_render_group_light_test const *test) {
  OwlByte *data;
  VkDeviceSize size;
  VkDescriptorSet sets[3];
  struct owl_tmp_submit_mem_ref vtx;
  struct owl_tmp_submit_mem_ref idx;
  struct owl_tmp_submit_mem_ref pvm;
  struct owl_tmp_submit_mem_ref lighting;
  OwlU32 uniform_offsets[2];
  OwlU32 const indices[] = {2, 3, 1, 1, 0, 2};
  OwlU32 const active = renderer->active_buf;

  size = sizeof(test->vertex);
  data = owl_alloc_tmp_submit_mem(renderer, size, &vtx);

  OWL_MEMCPY(data, test->vertex, size);

  size = sizeof(indices);
  data = owl_alloc_tmp_submit_mem(renderer, size, &idx);

  OWL_MEMCPY(data, indices, size);

  size = sizeof(test->pvm);
  data = owl_alloc_tmp_submit_mem(renderer, size, &pvm);

  OWL_MEMCPY(data, &test->pvm, size);

  size = sizeof(test->lighting);
  data = owl_alloc_tmp_submit_mem(renderer, size, &lighting);

  OWL_MEMCPY(data, &test->lighting, size);

  sets[0] = pvm.pvm_set;
  sets[1] = owl_get_texture(test->texture)->set;
  sets[2] = lighting.light_set;

  uniform_offsets[0] = pvm.offset32;
  uniform_offsets[1] = lighting.offset32;

  vkCmdBindVertexBuffers(renderer->draw_cmd_bufs[active], 0, 1, &vtx.buf,
                         &vtx.offset);

  vkCmdBindIndexBuffer(renderer->draw_cmd_bufs[active], idx.buf, idx.offset,
                       VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets(
      renderer->draw_cmd_bufs[active], VK_PIPELINE_BIND_POINT_GRAPHICS,
      renderer->pipeline_layouts[renderer->bound_pipeline], 0,
      OWL_ARRAY_SIZE(sets), sets, OWL_ARRAY_SIZE(uniform_offsets),
      uniform_offsets);

  vkCmdDrawIndexed(renderer->draw_cmd_bufs[active], OWL_ARRAY_SIZE(indices),
                   1, 0, 0, 0);

  return OWL_SUCCESS;
}
#endif

enum owl_code owl_submit_render_group(struct owl_vk_renderer *renderer,
                                      struct owl_render_command const *group) {
  switch (group->type) {
  case OWL_RENDER_COMMAND_TYPE_NOP:
    return OWL_SUCCESS;

  case OWL_RENDER_COMMAND_TYPE_BASIC:
    return owl_submit_render_group_basic_(renderer, &group->storage.as_basic);

  case OWL_RENDER_COMMAND_TYPE_QUAD:
    return owl_submit_render_group_quad_(renderer, &group->storage.as_quad);
  }
}

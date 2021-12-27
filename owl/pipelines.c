#include <owl/pipelines.h>
#include <owl/vk_renderer.h>

enum owl_code owl_bind_pipeline(struct owl_vk_renderer *renderer,
                                enum owl_pipeline_type type) {
  OwlU32 const active = renderer->dyn_active_buf;

  renderer->bound_pipeline = type;

  if (OWL_PIPELINE_TYPE_NONE == type)
    return OWL_SUCCESS;

  vkCmdBindPipeline(renderer->frame_cmd_bufs[active],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->pipelines[type]);

  return OWL_SUCCESS;
}

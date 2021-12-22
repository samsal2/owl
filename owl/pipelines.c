#include <owl/pipelines.h>
#include <owl/renderer_internal.h>

enum owl_code owl_bind_pipeline(struct owl_renderer *renderer,
                                enum owl_pipeline_type type) {
  OwlU32 const active = renderer->active_buf;

  renderer->bound_pipeline = type;

  if (OWL_PIPELINE_TYPE_NONE == type)
    return OWL_SUCCESS;

  vkCmdBindPipeline(renderer->draw_cmd_bufs[active],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->pipelines[type]);

  return OWL_SUCCESS;
}

#include <owl/pipelines.h>
#include <owl/renderer_internal.h>

enum owl_code owl_bind_pipeline(struct owl_renderer *renderer,
                                enum owl_pipeline_type type) {
  int const active = renderer->cmd.active;

  renderer->pipeline.bound = type;

  if (OWL_PIPELINE_TYPE_NONE == type)
    return OWL_SUCCESS;

  vkCmdBindPipeline(renderer->cmd.bufs[active],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    renderer->pipeline.as[type]);

  return OWL_SUCCESS;
}

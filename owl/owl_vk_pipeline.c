#include "owl_vk_pipeline.h"

#include "owl_vk_renderer.h"

owl_public owl_code
owl_vk_bind_pipeline(struct owl_vk_renderer *vk, enum owl_vk_pipeline id)
{
  if (vk->pipeline == id)
    return OWL_OK;

  vk->pipeline = id;

  if (OWL_VK_PIPELINE_NONE == id)
    return OWL_OK;

  if (0 > id || OWL_VK_NUM_PIPELINES < id)
    return OWL_ERROR_NOT_FOUND;

  vkCmdBindPipeline(vk->frame_command_buffers[vk->frame],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelines[id]);

  return OWL_OK;
}

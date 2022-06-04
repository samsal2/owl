#ifndef OWL_VK_PIPELINE_H_
#define OWL_VK_PIPELINE_H_

#include "owl-definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

enum owl_vk_pipeline {
  OWL_VK_PIPELINE_BASIC,
  OWL_VK_PIPELINE_WIRES,
  OWL_VK_PIPELINE_TEXT,
  OWL_VK_PIPELINE_MODEL,
  OWL_VK_PIPELINE_SKYBOX,
  OWL_VK_PIPELINE_NONE
};
#define OWL_VK_NUM_PIPELINES OWL_VK_PIPELINE_NONE

owl_public owl_code
owl_vk_bind_pipeline(struct owl_vk_renderer *vk, enum owl_vk_pipeline id);

OWL_END_DECLS

#endif

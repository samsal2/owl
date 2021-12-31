#ifndef OWL_INCLUDE_PIPELINES_H_
#define OWL_INCLUDE_PIPELINES_H_

#include "code.h"

struct owl_vk_renderer;

enum owl_pipeline_type {
  OWL_PIPELINE_TYPE_MAIN,
  OWL_PIPELINE_TYPE_WIRES,
  OWL_PIPELINE_TYPE_FONT,
  OWL_PIPELINE_TYPE_COUNT
};

#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

enum owl_code owl_bind_pipeline(struct owl_vk_renderer *renderer,
                                enum owl_pipeline_type type);

#endif

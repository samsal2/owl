#ifndef OWL_PIPELINES_H_
#define OWL_PIPELINES_H_

#include <owl/code.h>
#include <owl/fwd.h>
#include <owl/types.h>

enum owl_pipeline_type {
  OWL_PIPELINE_TYPE_MAIN,
  OWL_PIPELINE_TYPE_WIRES,
  OWL_PIPELINE_TYPE_FONT,
  OWL_PIPELINE_TYPE_LIGHT,
  OWL_PIPELINE_TYPE_COUNT
};

#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

enum owl_code owl_bind_pipeline(struct owl_renderer *renderer,
                                enum owl_pipeline_type type);

#endif

#ifndef OWL_VK_TYPES_H_
#define OWL_VK_TYPES_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_pcu_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_pnuujw_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_v4 joints0;
  owl_v4 weights0;
};

struct owl_pvm_ubo {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

OWL_END_DECLS

#endif

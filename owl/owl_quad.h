#ifndef OWL_QUAD_H_
#define OWL_QUAD_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_image;

struct owl_quad {
  owl_v2 position0;
  owl_v2 position1;
  owl_v3 color;
  owl_v2 uv0;
  owl_v2 uv1;
  struct owl_vk_image const *texture;
};

OWL_END_DECLS

#endif

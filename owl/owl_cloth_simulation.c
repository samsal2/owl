#include "owl_cloth_simulation.h"

#include "owl_internal.h"
#include "owl_vector_math.h"
#include "owl_vk_renderer.h"

owl_public owl_code
owl_cloth_init(struct owl_cloth *cloth, int w, int h) {
  owl_code code = OWL_OK;

  cloth->num_points = w * h;

  cloth->points = owl_malloc((size_t)(cloth->num_points));
  if (cloth->points) {
    int i;
    for (i = 0; i < cloth->num_points; ++i) {
    }
  } else {
    code = OWL_ERROR_NO_MEMORY:
  }

  return code;
}

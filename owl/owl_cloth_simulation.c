#include "owl_cloth_simulation.h"

#include "owl_internal.h"
#include "owl_vector_math.h"
#include "owl_vk_renderer.h"

OWL_PUBLIC owl_code
owl_cloth_init(struct owl_cloth *cloth, uint32_t w, uint32_t h)
{
  cloth->num_points = w * h;

  cloth->points = owl_malloc(cloth->num_points * sizeof(*cloth->points));
  if (cloth->points) {
    uint32_t i;
    for (i = 0; i < cloth->num_points; ++i) {
      uint32_t const x = i % w;
      uint32_t const y = i / w;
    }
  }
}

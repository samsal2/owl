#ifndef OWL_CLOTH_SIMULATION_H_
#define OWL_CLOTH_SIMULATION_H_

#include "owl_definitions.h"
#include "owl_vk_types.h"

OWL_BEGIN_DECLS

struct owl_cloth_point {
  int movable;
  owl_v3 position;
  owl_v3 velocity;
  owl_v3 acceleration;
  owl_v3 previous;
  owl_v4 distances;
  struct owl_cloth_point *links[4];
};

struct owl_cloth {
  uint32_t width;
  uint32_t height;

  uint32_t num_points;
  struct owl_cloth_point *points;

  uint32_t num_draw_indices;
  uint32_t *draw_indices;

  uint32_t num_draw_vertices;
  struct owl_pcu_vertex *draw_vertices;
};

OWL_PUBLIC owl_code
owl_cloth_init(struct owl_cloth *cloth, uint32_t w, uint32_t h);

OWL_PUBLIC void
owl_cloth_deinit(struct owl_cloth *cloth);

OWL_PUBLIC owl_code
owl_draw_cloth(struct owl_cloth *cloth);

OWL_PUBLIC owl_code
owl_cloth_update(struct owl_cloth *cloth, float dt);

OWL_END_DECLS

#endif

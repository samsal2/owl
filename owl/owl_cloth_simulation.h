#ifndef OWL_CLOTH_SIMULATION_H_
#define OWL_CLOTH_SIMULATION_H_

#include "owl_definitions.h"
#include "owl_vector.h"
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
  int width;
  int height

      int num_points;
  struct owl_cloth_point *points;
};

owl_public owl_code
owl_cloth_init(struct owl_cloth *cloth, int w, int h);

owl_public void
owl_cloth_deinit(struct owl_cloth *cloth);

owl_public owl_code
owl_draw_cloth(struct owl_cloth *cloth);

owl_public owl_code
owl_cloth_update(struct owl_cloth *cloth, float dt);

OWL_END_DECLS

#endif

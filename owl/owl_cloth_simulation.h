#ifndef OWL_CLOTH_SIMULATION_H_
#define OWL_CLOTH_SIMULATION_H_

#include "owl_definitions.h"

#include "owl_texture_2d.h"

OWL_BEGIN_DECLS

struct owl_renderer;

struct owl_cloth_particle {
  int32_t movable;
  owl_v3 position;
  owl_v3 previous_position;
  owl_v3 velocity;
  owl_v3 acceleration;
  float distances[4];
  struct owl_cloth_particle *links[4];
};

struct owl_cloth_simulation {
  int32_t width;
  int32_t height;
  owl_m4 model;

  int32_t particle_count;
  struct owl_cloth_particle *particles;

  struct owl_texture_2d material;
};

OWL_PUBLIC owl_code
owl_cloth_simulation_init(struct owl_cloth_simulation *sim,
                          struct owl_renderer *renderer, int32_t width,
                          int32_t height, char const *material);

OWL_PUBLIC void
owl_cloth_simulation_deinit(struct owl_cloth_simulation *sim,
                            struct owl_renderer *renderer);

OWL_PUBLIC void
owl_cloth_simulation_update(struct owl_cloth_simulation *sim, float dt);

OWL_END_DECLS

#endif

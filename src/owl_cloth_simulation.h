#ifndef OWL_CLOTH_SIMULATION_H
#define OWL_CLOTH_SIMULATION_H

#include "owl_definitions.h"

#include "owl_texture.h"

OWL_BEGIN_DECLARATIONS

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

    int32_t num_particles;
    struct owl_cloth_particle *particles;

    struct owl_texture material;
};

OWLAPI int owl_cloth_simulation_init(struct owl_cloth_simulation *sim,
                                     struct owl_renderer *r, int32_t width,
                                     int32_t height, char const *material);

OWLAPI void owl_cloth_simulation_deinit(struct owl_cloth_simulation *sim,
                                        struct owl_renderer *r);

OWLAPI void owl_cloth_simulation_update(struct owl_cloth_simulation *sim,
                                        float dt);

OWL_END_DECLARATIONS

#endif

#include "owl_cloth_simulation.h"

#include "owl_internal.h"
#include "owl_renderer.h"
#include "owl_vector_math.h"

#define OWL_GRAVITY 0.05F
#define OWL_DAMPING 0.002F
#define OWL_STEPS 4

OWL_PUBLIC owl_code
owl_cloth_simulation_init(struct owl_cloth_simulation *sim,
                          struct owl_renderer *renderer, int32_t width,
                          int32_t height, char const *material) {
  int32_t i;
  int32_t j;
  owl_code code;
  struct owl_texture_2d_desc texture_desc;

  texture_desc.source = OWL_RENDERER_TEXTURE_SOURCE_FILE;
  texture_desc.file = material;

  code = owl_texture_2d_init(&sim->material, renderer, &texture_desc);
  if (code)
    goto out;

  sim->width = width;
  sim->height = height;
  sim->particle_count = width * height;

  sim->particles = OWL_MALLOC(sim->particle_count * sizeof(*sim->particles));
  if (!sim->particles) {
    code = OWL_ERROR_NO_MEMORY;
    goto error_deinit_material;
  }

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      float const x = 2.0F * (float)j / (float)(width - 1) - 1.0F;
      float const y = 2.0F * (float)i / (float)(height - 1) - 1.0F;
      struct owl_cloth_particle *particle = &sim->particles[i * width + j];

      particle->movable = i != 0;
      OWL_V3_SET(particle->position, x, y, 0.0F);
      OWL_V3_COPY(particle->position, particle->previous_position);
      OWL_V3_SET(particle->velocity, 0.0F, 0.0F, 0.0F);
      OWL_V3_SET(particle->acceleration, 0.0F, OWL_GRAVITY, 0.08F);
    }
  }

  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      struct owl_cloth_particle *particle = &sim->particles[i * width + j];

      if (0 < j) {
        struct owl_cloth_particle *link = &sim->particles[i * width + (j - 1)];

        particle->links[0] = link;
        particle->distances[0] = owl_v3_distance(particle->position,
                                                 link->position);
      } else {
        particle->links[0] = NULL;
        particle->distances[0] = 0.0F;
      }

      if (0 < i) {
        struct owl_cloth_particle *link = &sim->particles[(i - 1) * width + j];

        particle->links[1] = link;
        particle->distances[1] = owl_v3_distance(particle->position,
                                                 link->position);
      } else {
        particle->links[1] = NULL;
        particle->distances[1] = 0.0F;
      }

      if ((width - 1) > j) {
        struct owl_cloth_particle *link = &sim->particles[i * width + (j + 1)];

        particle->links[2] = link;
        particle->distances[2] = owl_v3_distance(particle->position,
                                                 link->position);
      } else {
        particle->links[2] = NULL;
        particle->distances[2] = 0.0F;
      }

      if ((height - 1) > i) {
        struct owl_cloth_particle *link = &sim->particles[(i + 1) * width + j];

        particle->links[3] = link;
        particle->distances[3] = owl_v3_distance(particle->position,
                                                 link->position);
      } else {
        particle->links[3] = NULL;
        particle->distances[3] = 0.0F;
      }
    }
  }

  {
    owl_v3 position = {0.0F, 0.0F, -1.0F};
    OWL_V4_IDENTITY(sim->model);
    owl_m4_translate(position, sim->model);
  }

  goto out;

error_deinit_material:
  owl_texture_2d_deinit(&sim->material, renderer);

out:
  return code;
}

OWL_PUBLIC void
owl_cloth_simulation_deinit(struct owl_cloth_simulation *sim,
                            struct owl_renderer *renderer) {
  vkDeviceWaitIdle(renderer->device);

  OWL_FREE(sim->particles);
  owl_texture_2d_deinit(&sim->material, renderer);
}

OWL_PUBLIC void
owl_cloth_simulation_update(struct owl_cloth_simulation *sim, float dt) {
  int32_t i;
  for (i = 0; i < sim->particle_count; ++i) {
    struct owl_cloth_particle *particle = &sim->particles[i];
    if (particle->movable) {
      int32_t j;
      owl_v3 delta_position;

      particle->acceleration[1] += OWL_GRAVITY;

      /* velocity = position - previous_position */
      OWL_V3_SUB(particle->position, particle->previous_position,
                 particle->velocity);

      /* velocity *= 1.0F - OWL_DAMPING */
      OWL_V3_SCALE(particle->velocity, 1.0F - OWL_DAMPING, particle->velocity);

      /* delta_position = velocity + acceleration * dt */
      OWL_V3_SCALE(particle->acceleration, dt, delta_position);
      OWL_V3_ADD(delta_position, particle->velocity, delta_position);

      /* new previous position */
      OWL_V3_COPY(particle->position, particle->previous_position);

      /* position = previous_position + delta_position */
      OWL_V3_ADD(particle->previous_position, delta_position,
                 particle->position);

      /* reset acceleration */
      OWL_V3_ZERO(particle->acceleration);

      /* contraint the particle links */
      for (j = 0; j < OWL_STEPS; ++j) {
        int32_t side;
        for (side = 0; side < 4; ++side) {
          struct owl_cloth_particle *link = particle->links[side];
          if (link) {
            float factor;
            owl_v3 delta;
            owl_v3 correction;

            /* get the distance between the newly calculated position and the
             * link position */
            OWL_V3_SUB(link->position, particle->position, delta);

            /* calculate the contraints factor */
            factor = 1 - (particle->distances[side] / owl_v3_magnitude(delta));

            /* find the correction value */
            OWL_V3_SCALE(delta, factor, correction);

            /* half the correction value and apply it to the particle */
            OWL_V3_SCALE(correction, 0.5F, correction);
            OWL_V3_ADD(particle->position, correction, particle->position);

            /* if the link is movable, apply the other half of the correction
             */
            if (link->movable)
              OWL_V3_SUB(link->position, correction, link->position);
          }
        }
      }
    }
  }
}

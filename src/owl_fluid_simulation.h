#ifndef OWL_FLUID_SIMULATION_H
#define OWL_FLUID_SIMULATION_H

#include "owl_renderer.h"

OWL_BEGIN_DECLARATIONS

struct owl_fluid_simulation_uniform {
    owl_v2 point;
    float radius;
    float aspect_ratio;

    owl_v2 sim_texel_size;
    owl_v2 dye_texel_size;

    owl_v4 color;

    float dt;
    float curl;
};

struct owl_fluid_simulation_push_constant {
    owl_v2 texel_size;
};

struct owl_fluid_simulation_buffer {
    VkDescriptorSet storage_descriptor_set;
    struct owl_texture texture;
};

struct owl_fluid_simulation {
    int32_t requires_record;

    owl_v2 sim_texel_size;
    owl_v2 dye_texel_size;

    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    VkDescriptorSet uniform_descriptor_set;
    struct owl_fluid_simulation_uniform *uniform;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

    int32_t velocity_read;
    int32_t velocity_write;
    struct owl_fluid_simulation_buffer velocity[2];

    int32_t pressure_read;
    int32_t pressure_write;
    struct owl_fluid_simulation_buffer pressure[2];

    int32_t dye_read;
    int32_t dye_write;
    struct owl_fluid_simulation_buffer dye[2];

    struct owl_fluid_simulation_buffer curl;
    struct owl_fluid_simulation_buffer divergence;
};

/* TODO(samuel): simulation parameters */
OWLAPI int owl_fluid_simulation_init(struct owl_fluid_simulation *sim,
                                     struct owl_renderer *r);

OWLAPI void owl_fluid_simulation_update(struct owl_fluid_simulation *sim,
                                        struct owl_renderer *r, float dt);

OWLAPI void owl_fluid_simulation_deinit(struct owl_fluid_simulation *sim,
                                        struct owl_renderer *r);

OWL_END_DECLARATIONS

#endif

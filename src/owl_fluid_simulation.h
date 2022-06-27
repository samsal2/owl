#ifndef OWL_FLUID_SIMULATION_H
#define OWL_FLUID_SIMULATION_H

#include "owl_texture.h"

OWL_BEGIN_DECLARATIONS

struct owl_fluid_simulation_push_constant {
  owl_v2 point;
  owl_v2 padding0;

  owl_v4 color;

  owl_v2 dye_dimensions;
  owl_v2 sim_dimensions;

  float dt;
  float dissipation;
  owl_v2 texel_size;
  
  owl_v2 dye_texel_size;
  owl_v2 sim_texel_size;

  float aspect_ratio;
  float decay;
  float curl;
  float padding1;
};

struct owl_fluid_simulation_buffer {
  VkDescriptorSet storage_descriptor_set;
  struct owl_texture texture;
};

struct owl_fluid_simulation {
  VkSemaphore semaphore;

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

OWL_END_DECLARATIONS

#endif

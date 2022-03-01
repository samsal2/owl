#ifndef OWL_SKYBOX_H_
#define OWL_SKYBOX_H_

#include "types.h"

#include <vulkan/vulkan.h>

#define OWL_SKYBOX_FACE_COUNT 6

struct owl_renderer;

struct owl_skybox_vertex {
  owl_v3 position;
};

struct owl_skybox_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

struct owl_skybox_init_info {
  char const *right;
  char const *left;
  char const *top;
  char const *bottom;
  char const *back;
  char const *front;
};

struct owl_skybox {
  owl_u32 mips;
  VkImage image;
  VkImageView view;
  VkDeviceMemory memory;
  VkSampler sampler;
  VkDescriptorSet set;
};

enum owl_code owl_skybox_init(struct owl_renderer *r,
                              struct owl_skybox_init_info const *sli,
                              struct owl_skybox *box);

void owl_skybox_deinit(struct owl_renderer *r, struct owl_skybox *box);

#endif

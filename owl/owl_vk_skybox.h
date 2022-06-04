#ifndef OWL_VK_SKYBOX_H_
#define OWL_VK_SKYBOX_H_

#include "owl_definitions.h"

struct owl_vk_renderer;

owl_code
owl_vk_skybox_load(struct owl_vk_renderer *vk, char const *path);

owl_public void
owl_vk_skybox_unload(struct owl_vk_renderer *vk);

#endif

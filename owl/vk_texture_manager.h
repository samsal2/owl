#ifndef OWL_VK_TEXTURE_MANAGER_H_
#define OWL_VK_TEXTURE_MANAGER_H_

#include <owl/vk_texture.h>

#define OWL_VK_TEXTURE_COUNT 16

struct owl_vk_texture_manager {
  int current;
  int slots[OWL_VK_TEXTURE_COUNT];
  struct owl_vk_texture textures[OWL_VK_TEXTURE_COUNT];
};

enum owl_code
owl_init_texture_manager(struct owl_vk_renderer const *renderer,
                         struct owl_vk_texture_manager *manager);

void owl_deinit_texture_manager(struct owl_vk_renderer const *renderer,
                                struct owl_vk_texture_manager *manager);

struct owl_vk_texture *
owl_request_texture_mem(struct owl_vk_texture_manager *manager,
                        OwlTexture *handle);

struct owl_vk_texture *
owl_release_texture_mem(struct owl_vk_texture_manager *manager,
                       OwlTexture handle);

#endif

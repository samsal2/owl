#include <owl/internal.h>
#include <owl/vk_texture_manager.h>

enum owl_code
owl_init_texture_manager(struct owl_vk_renderer const *renderer,
                         struct owl_vk_texture_manager *manager) {
  int i;

  manager->current = 0;

  OWL_UNUSED(renderer);

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    manager->slots[i] = 0;

  return OWL_SUCCESS;
}

void owl_deinit_texture_manager(struct owl_vk_renderer const *renderer,
                                struct owl_vk_texture_manager *manager) {
  int i;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (manager->slots[i])
      owl_deinit_vk_texture(renderer, &manager->textures[i]);
}

struct owl_vk_texture *
owl_request_texture_mem(struct owl_vk_texture_manager *manager,
                        OwlTexture *handle) {
  int i;

  if (OWL_INVALID_TEXTURE == (*handle = manager->current))
    return NULL;

  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i)
    if (!manager->slots[i])
      manager->current = i;
  manager->slots[manager->current] = 1;

  /* find the next empty slot and set current to it */
  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i) {
    if (!manager->slots[i]) {
      manager->current = i;
      break;
    }
  }
      

  /* if no slot was found, current is invalid */
  if (OWL_VK_TEXTURE_COUNT == i)
    manager->current = OWL_INVALID_TEXTURE;

  return &manager->textures[*handle];
}

struct owl_vk_texture *
owl_release_texture_mem(struct owl_vk_texture_manager *manager,
                        OwlTexture handle) {
  int i;

  /* validate the texture */
  if (0 > handle || OWL_VK_TEXTURE_COUNT <= handle)
    return NULL;

  /* mark the slot as freed */
  manager->slots[handle] = 0;

  /* find the first empty slot */
  for (i = 0; i < OWL_VK_TEXTURE_COUNT; ++i) {
    if (!manager->slots[i]) {
      manager->current = i;
      break;
    }
  }

  /* return the memory so it can be deinited */
  return &manager->textures[handle];
}

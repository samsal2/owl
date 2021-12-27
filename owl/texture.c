#include <owl/internal.h>
#include <owl/texture.h>
#include <owl/vk_texture.h>
#include <owl/vk_texture_manager.h>
#include <stb/stb_image.h>

OWL_GLOBAL int g_is_texture_manager_active;
OWL_GLOBAL struct owl_vk_texture_manager g_texture_manager;

OWL_INTERNAL void
owl_ensure_manager_(struct owl_vk_renderer const *renderer) {
  if (g_is_texture_manager_active)
    return;

  owl_init_texture_manager(renderer, &g_texture_manager);
  g_is_texture_manager_active = 1;
}

enum owl_code owl_create_texture(struct owl_vk_renderer *renderer, int width,
                                 int height, OwlByte const *data,
                                 enum owl_pixel_format format,
                                 enum owl_sampler_type sampler,
                                 OwlTexture *texture) {
  struct owl_vk_texture *mem;
  enum owl_code err = OWL_SUCCESS;

  owl_ensure_manager_(renderer);

  /* TODO: check for NULL */
  mem = owl_request_texture_mem(&g_texture_manager, texture);
  err = owl_init_vk_texture(renderer, width, height, data, format, sampler,
                            mem);

  if (OWL_SUCCESS != err)
    owl_release_texture_mem(&g_texture_manager, *texture);

  return err;
}

enum owl_code owl_create_texture_from_file(struct owl_vk_renderer *renderer,
                                           char const *path,
                                           enum owl_sampler_type sampler,
                                           OwlTexture *texture) {
  OwlByte *data;
  int width;
  int height;
  int channels;
  enum owl_code err = OWL_SUCCESS;

  data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);

  if (!data)
    return OWL_ERROR_BAD_ALLOC;

  err = owl_create_texture(renderer, width, height, data,
                           OWL_PIXEL_FORMAT_R8G8B8A8_SRGB, sampler, texture);

  stbi_image_free(data);

  return err;
}

void owl_destroy_texture(struct owl_vk_renderer const *renderer,
                         OwlTexture texture) {

  struct owl_vk_texture *mem;
  owl_ensure_manager_(renderer);

  mem = owl_release_texture_mem(&g_texture_manager, texture);

  owl_deinit_vk_texture(renderer, mem);
}

struct owl_vk_texture_manager *
owl_peek_texture_manager_(struct owl_vk_renderer const *renderer) {
#if 0
  owl_ensure_manager_(renderer);
#else
  OWL_UNUSED(renderer);
#endif
  return &g_texture_manager;
}

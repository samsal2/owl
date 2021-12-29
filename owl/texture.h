#ifndef OWL_TEXTURE_H_
#define OWL_TEXTURE_H_

#include <owl/code.h>
#include <owl/fwd.h>
#include <owl/types.h>

#define OWL_INVALID_TEXTURE -1

enum owl_sampler_type {
  OWL_SAMPLER_TYPE_LINEAR,
  OWL_SAMPLER_TYPE_CLAMP,
  OWL_SAMPLER_TYPE_COUNT
};

enum owl_sampler_filter {
  OWL_SAMPLER_FILTER_NEAREST,
  OWL_SAMPLER_FILTER_LINEAR
};

enum owl_sampler_mip_mode {
  OWL_SAMPLER_MIP_MODE_NEAREST,
  OWL_SAMPLER_MIP_MODE_LINEAR
};

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_sampler_addr_mode {
  OWL_SAMPLER_ADDR_MODE_REPEAT,
  OWL_SAMPLER_ADDR_MODE_MIRRORED_REPEAT,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER
};

struct owl_texture_attr {
  int width;
  int height;
  enum owl_pixel_format format;
  enum owl_sampler_mip_mode mip_mode;
  enum owl_sampler_filter min_filter;
  enum owl_sampler_filter mag_filter;
  enum owl_sampler_addr_mode wrap_u;
  enum owl_sampler_addr_mode wrap_v;
  enum owl_sampler_addr_mode wrap_w;
};

enum owl_code owl_create_texture(struct owl_vk_renderer *renderer,
                                 struct owl_texture_attr const *attr,
                                 OwlByte const *data, OwlTexture *texture);

enum owl_code owl_create_texture_from_file(struct owl_vk_renderer *renderer,
                                           struct owl_texture_attr *attr,
                                           char const *path,
                                           OwlTexture *texture);

void owl_destroy_texture(struct owl_vk_renderer const *renderer,
                         OwlTexture texture);

struct owl_vk_texture_manager *
owl_peek_texture_manager_(struct owl_vk_renderer const *renderer);

#endif

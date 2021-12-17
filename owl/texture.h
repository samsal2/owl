#ifndef OWL_TEXTURE_H_
#define OWL_TEXTURE_H_

#include <owl/fwd.h>
#include <owl/types.h>

#define OWL_INVALID_TEXTURE -1

enum owl_sampler_type {
  OWL_SAMPLER_TYPE_LINEAR,
  OWL_SAMPLER_TYPE_NEAREST,
  OWL_SAMPLER_TYPE_COUNT
};

enum owl_code owl_create_texture(struct owl_renderer *renderer, int width,
                                 int height, OwlByte const *data,
                                 enum owl_pixel_format format,
                                 enum owl_sampler_type sampler,
                                 OwlTexture *texture);

enum owl_code owl_create_texture_from_file(struct owl_renderer *renderer,
                                           char const *path,
                                           enum owl_sampler_type sampler,
                                           OwlTexture *texture);

void owl_destroy_texture(struct owl_renderer const *renderer,
                         OwlTexture texture);

#endif

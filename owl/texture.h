#ifndef OWL_TEXTURE_H_
#define OWL_TEXTURE_H_

#include <owl/types.h>

#define OWL_INVALID_TEXTURE -1

struct owl_renderer;

enum owl_code owl_create_texture(struct owl_renderer *renderer, int width,
                                 int height, OwlByte const *data,
                                 enum owl_pixel_format format,
                                 OwlTexture *texture);

enum owl_code owl_create_texture_from_file(struct owl_renderer *renderer,
                                           char const *path,
                                           OwlTexture *texture);

void owl_destroy_texture(struct owl_renderer const *renderer,
                         OwlTexture texture);

#endif

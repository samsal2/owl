#ifndef OWL_INCLUDE_IMG_H_
#define OWL_INCLUDE_IMG_H_

#include "code.h"

struct owl_img;
struct owl_vk_renderer;

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

struct owl_img_desc {
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

enum owl_code owl_create_img(struct owl_vk_renderer *renderer,
                             struct owl_img_desc const *desc,
                             unsigned char const *data, struct owl_img **img);

enum owl_code owl_create_img_from_file(struct owl_vk_renderer *renderer,
                                       struct owl_img_desc *desc,
                                       char const *path, struct owl_img **img);

void owl_destroy_img(struct owl_vk_renderer const *renderer,
                     struct owl_img *img);

#endif

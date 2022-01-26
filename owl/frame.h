#ifndef OWL_FRAME_H_
#define OWL_FRAME_H_

#include "types.h"

struct owl_vk_renderer;

enum owl_code owl_frame_begin(struct owl_vk_renderer *r);
enum owl_code owl_frame_end(struct owl_vk_renderer *r);

#endif

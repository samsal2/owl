#ifndef OWL_INCLUDE_FRAME_H_
#define OWL_INCLUDE_FRAME_H_

#include "code.h"

struct owl_vk_renderer;

enum owl_code owl_begin_frame(struct owl_vk_renderer *renderer);
enum owl_code owl_end_frame(struct owl_vk_renderer *renderer);

#endif

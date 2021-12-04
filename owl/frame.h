#ifndef OWL_FRAME_H_
#define OWL_FRAME_H_

#include <owl/types.h>

struct owl_renderer;

enum owl_code owl_begin_frame(struct owl_renderer *renderer);

enum owl_code owl_end_frame(struct owl_renderer *renderer);

#endif

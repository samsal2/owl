#ifndef OWL_FRAME_H_
#define OWL_FRAME_H_

#include "types.h"

struct owl_renderer;

enum owl_code owl_frame_begin(struct owl_renderer *r);
enum owl_code owl_frame_end(struct owl_renderer *r);

#endif

#ifndef OWL_FRAME_H_
#define OWL_FRAME_H_

#include <owl/fwd.h>
#include <owl/code.h>
#include <owl/types.h>

enum owl_code owl_begin_frame(struct owl_renderer *renderer);
enum owl_code owl_end_frame(struct owl_renderer *renderer);

#endif

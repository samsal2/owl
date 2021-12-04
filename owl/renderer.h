#ifndef OWL_RENDERER_H_
#define OWL_RENDERER_H_

#include <owl/types.h>

#ifndef NDEBUG
#define OWL_ENABLE_VALIDATION
#endif

struct owl_renderer;
struct owl_window;

enum owl_code owl_create_renderer(struct owl_window const *window,
                                  struct owl_renderer **renderer);

enum owl_code owl_recreate_renderer(struct owl_window const *window,
                                    struct owl_renderer *renderer);

void owl_destroy_renderer(struct owl_renderer *renderer);

#endif

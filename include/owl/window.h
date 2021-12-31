#ifndef OWL_INCLUDE_WINDOW_H_
#define OWL_INCLUDE_WINDOW_H_

#include "code.h"

struct owl_window;
struct owl_input_state;

enum owl_code owl_create_window(int width, int height, char const *title,
                                struct owl_input_state **input,
                                struct owl_window **window);

void owl_destroy_window(struct owl_window *window);

void owl_poll_window_input(struct owl_window *window);

int owl_is_window_done(struct owl_window *window);

#endif

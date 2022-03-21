#ifndef OWL_IMGUI_H_
#define OWL_IMGUI_H_

#include "types.h"

struct owl_renderer;
struct owl_client;

enum owl_code owl_imgui_init(struct owl_client const *c,
                             struct owl_renderer const *r, char const *font);

void owl_imgui_deinit(struct owl_renderer const *r);

enum owl_code owl_imgui_handle_resize(struct owl_client const *c,
                                      struct owl_renderer const *r);

enum owl_code owl_imgui_begin_frame(struct owl_renderer const *r);

enum owl_code owl_imgui_end_frame(struct owl_renderer const *r);

void owl_imgui_begin(char const *title);

void owl_imgui_end(void);

void owl_imgui_text(char const *fmt, ...);

#endif

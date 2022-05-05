#ifndef OWL_IMGUI_H_
#define OWL_IMGUI_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "owl_types.h"

struct owl_renderer;
struct owl_client;

struct owl_imgui {
  void *data_;
};

enum owl_code owl_imgui_init(struct owl_client *c, struct owl_renderer *r,
                             struct owl_imgui *im);

void owl_imgui_deinit(struct owl_renderer *r, struct owl_imgui *im);

void owl_imgui_begin_frame(struct owl_imgui *im);

void owl_imgui_end_frame(struct owl_imgui *im);

void owl_imgui_begin(struct owl_imgui *im, char const *title);

void owl_imgui_end(struct owl_imgui *im);

void owl_imgui_text(struct owl_imgui *im, char const *fmt, ...);

void owl_imgui_checkbox(struct owl_imgui *im, char const *text, int *state);

void owl_imgui_sliderf(struct owl_imgui *im, char const *text, float *state,
                       float begin, float end);

void owl_imgui_color_edit_v3(struct owl_imgui *im, char const *text,
                             owl_v3 color);

int owl_imgui_button(struct owl_imgui *im, char const *text);

void owl_imgui_same_line(struct owl_imgui *im);

void owl_imgui_render(struct owl_imgui *im, struct owl_renderer *r);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

#ifndef OWL_IMGUI_H_
#define OWL_IMGUI_H_

/* owl_imgui.h - custom imgui c bindings */

#if defined(__cplusplus)
extern "C" {
#endif

#include "owl_types.h"

#include <vulkan/vulkan.h>

struct owl_renderer;
struct owl_client;

#define OWL_IMGUI_MAX_TEXTURE_SETS_COUNT 32

struct owl_imgui {
  void *data_;
};

enum owl_imgui_window_flags {
  OWL_IMGUI_WINDOW_FLAGS_NONE = 0,
  OWL_IMGUI_WINDOW_FLAGS_NO_TITLE_BAR = 1 << 0,
  OWL_IMGUI_WINDOW_FLAGS_NO_RESIZE = 1 << 1,
  OWL_IMGUI_WINDOW_FLAGS_NO_MOVE = 1 << 2,
  OWL_IMGUI_WINDOW_FLAGS_NO_SCROLL_BAR = 1 << 3,
  OWL_IMGUI_WINDOW_FLAGS_NO_SCROLL_WITH_MOUSE = 1 << 4,
  OWL_IMGUI_WINDOW_FLAGS_NO_COLLAPSE = 1 << 5
  /* TODO(samuel): missing flags */
};
typedef owl_i32 owl_imgui_window_flag_bits;

enum owl_imgui_cond_flags {
  OWL_IMGUI_COND_FLAGS_NONE = 0,
  OWL_IMGUI_COND_FLAGS_ALWAYS = 1 << 0,
  OWL_IMGUI_COND_FLAGS_ONCE = 1 << 1,
  OWL_IMGUI_COND_FLAGS_FIRST_USE_EVER = 1 << 2,
  OWL_IMGUI_COND_APPEARING = 1 << 3
};
typedef owl_i32 owl_imgui_cond_flag_bits;

enum owl_code owl_imgui_init(struct owl_client *c, struct owl_renderer *r,
                             struct owl_imgui *im);

void owl_imgui_deinit(struct owl_renderer *r, struct owl_imgui *im);

void owl_imgui_begin_frame(struct owl_imgui *im);

void owl_imgui_end_frame(struct owl_imgui *im);

void owl_imgui_begin(struct owl_imgui *im, char const *title,
                     owl_imgui_window_flag_bits bits);

void owl_imgui_end(struct owl_imgui *im);

void owl_imgui_text(struct owl_imgui *im, char const *fmt, ...);

void owl_imgui_checkbox(struct owl_imgui *im, char const *text, int *state);

void owl_imgui_slider_float(struct owl_imgui *im, char const *text,
                            float *state, float begin, float end);

void owl_imgui_color_edit_v3(struct owl_imgui *im, char const *text,
                             owl_v3 color);

int owl_imgui_button(struct owl_imgui *im, char const *text);

void owl_imgui_same_line(struct owl_imgui *im);

void owl_imgui_render(struct owl_imgui *im, struct owl_renderer *r);

float owl_imgui_framerate(struct owl_imgui *im);

void owl_imgui_demo_window(struct owl_imgui *im);

void owl_imgui_set_next_window_position(struct owl_imgui *im, owl_v2 pos,
                                        owl_imgui_cond_flag_bits cond,
                                        owl_v2 pivot);

void owl_imgui_set_next_window_size(struct owl_imgui *im, owl_v2 size,
                                    owl_imgui_cond_flag_bits cond);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif

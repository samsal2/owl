#include "owl_ui_stats.h"

#include "owl_imgui.h"
#include "owl_renderer.h"

void owl_ui_stats_draw(struct owl_renderer *r, struct owl_imgui *im) {
  owl_imgui_window_flag_bits flags = OWL_IMGUI_WINDOW_FLAGS_NONE;
  owl_v2 position = {-1.0F, -1.0F};
  owl_v2 pivot = {0.0F, 0.0F};
  owl_v2 size = {150.0F, r->window_height};

  owl_imgui_set_next_window_position(im, position, 0, pivot);
  owl_imgui_set_next_window_size(im, size, 0);

  flags |= OWL_IMGUI_WINDOW_FLAGS_NO_RESIZE;
  flags |= OWL_IMGUI_WINDOW_FLAGS_NO_MOVE;
  flags |= OWL_IMGUI_WINDOW_FLAGS_NO_COLLAPSE;

  owl_imgui_begin(im, "Stats", flags);
  { owl_imgui_text(im, "Framerate %.2F", owl_imgui_framerate(im)); }
  owl_imgui_end(im);

  owl_imgui_render(im, r);
}

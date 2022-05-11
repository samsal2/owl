#include "owl.h"

#include "owl_internal.h"

#include <stdio.h>

owl_public void
owl_ui_renderer_stats_draw (struct owl_vk_renderer *vkr)
{
  char buffer[256];
  owl_v2 position;
  owl_v3 color;

  snprintf (buffer, sizeof (buffer), "framerate: 0.0 fps");

  position[0] = -1.0F;
  position[1] = -0.98F;

  color[0] = 1.0F;
  color[1] = 1.0F;
  color[2] = 1.0F;

  owl_vk_renderer_draw_text (vkr, buffer, position, color);

#if 0
  if ((r->active_frame - 1) < 0) {
    snprintf (
        buffer, sizeof (buffer), "frame_heap_offset: %llu bytes",
        r->frame_heap_offsets[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT - 1]);
  } else {
    snprintf (buffer, sizeof (buffer), "frame_heap_offset: %llu bytes",
              r->frame_heap_offsets[r->active_frame - 1]);
  }

  position[1] = -0.98F * (1 - (0.08 * 1));

  owl_renderer_text_draw (r, position, color, buffer);

  snprintf (buffer, sizeof (buffer), "frame_heap_buffer_size: %llu bytes",
            r->frame_heap_buffer_size);

  position[1] = -0.98F * (1 - (0.08 * 2));

  owl_renderer_text_draw (r, position, color, buffer);
#endif
}

owl_public void
owl_ui_model_stats_draw (struct owl_vk_renderer *vkr, struct owl_model *m)
{
  owl_unused (vkr);
  owl_unused (m);
}

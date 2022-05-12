#include "owl_ui.h"

#include "owl_internal.h"
#include "owl_vk_renderer.h"

#include <stdio.h>

owl_public void
owl_ui_renderer_stats_draw (struct owl_vk_renderer *vkr)
{
  char buffer[256];
  owl_v2 position;
  owl_v3 color;

  struct owl_vk_frame *frame = owl_vk_renderer_frame_get (vkr);

  snprintf (buffer, sizeof (buffer), "framerate: %2.f fps",
            1 / (vkr->current_time - vkr->previous_time));

  position[0] = -1.0F;
  position[1] = -0.98F;

  color[0] = 1.0F;
  color[1] = 1.0F;
  color[2] = 1.0F;

  owl_vk_renderer_draw_text (vkr, buffer, position, color);

  position[1] = -0.88F;

  snprintf (buffer, sizeof (buffer), "frame->heap.size: %llu bytes",
            frame->heap.size);

  owl_vk_renderer_draw_text (vkr, buffer, position, color);

  position[1] = -0.78F;

  snprintf (buffer, sizeof (buffer), "frame->heap.offset: %llu",
            vkr->frames[!!!vkr->frame].heap.offset);

  owl_vk_renderer_draw_text (vkr, buffer, position, color);
}

owl_public void
owl_ui_model_stats_draw (struct owl_vk_renderer *vkr, struct owl_model *m)
{
  owl_unused (vkr);
  owl_unused (m);
}

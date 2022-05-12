#include "owl.h"

#include "owl_internal.h"

#include <stdio.h>

owl_public void
owl_ui_renderer_stats_draw (struct owl_vk_renderer *vkr)
{
  char buffer[256];
  owl_v2 position;
  owl_v3 color;

  snprintf (buffer, sizeof (buffer), "framerate: %2.f fps",
            1 / (vkr->current_time - vkr->previous_time));

  position[0] = -1.0F;
  position[1] = -0.98F;

  color[0] = 1.0F;
  color[1] = 1.0F;
  color[2] = 1.0F;

  owl_vk_renderer_draw_text (vkr, buffer, position, color);
}

owl_public void
owl_ui_model_stats_draw (struct owl_vk_renderer *vkr, struct owl_model *m)
{
  owl_unused (vkr);
  owl_unused (m);
}

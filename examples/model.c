#include <owl/owl.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static double prev_time_stamp;
static double time_stamp;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_model *model;
static struct owl_vk_font *font;
static owl_m4 matrix;

#define CHECK(fn)                                                             \
  do {                                                                        \
    enum owl_code code = (fn);                                                \
    if (OWL_SUCCESS != (code)) {                                              \
      printf ("something went wrong in call: %s, code %i\n", (#fn), code);    \
      return 0;                                                               \
    }                                                                         \
  } while (0)

int
main (void)
{
  owl_v3 offset = {0.0F, 0.0F, -1.0F};

  window = malloc (sizeof (*window));
  CHECK (owl_window_init (window, 600, 600, "model"));

  renderer = malloc (sizeof (*renderer));
  CHECK (owl_vk_renderer_init (renderer, window));

  model = malloc (sizeof (*model));
  CHECK (owl_model_init (model, renderer, "../../assets/CesiumMan.gltf"));

  font = malloc (sizeof (*font));
  CHECK (owl_vk_font_init (font, &renderer->context, &renderer->stage_heap,
                           "../../assets/Inconsolata-Regular.ttf", 64.0F));

  owl_vk_renderer_font_set (renderer, font);

  owl_m4_identity (matrix);
  owl_m4_translate (offset, matrix);

  prev_time_stamp = 0;
  time_stamp = 0;

  while (!owl_window_is_done (window)) {
    enum owl_code code;
    prev_time_stamp = time_stamp;
    time_stamp = owl_io_time_stamp_get ();

    code = owl_vk_renderer_frame_begin (renderer);
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == code) {
      owl_i32 w, h;
      owl_window_get_framebuffer_size (window, &w, &h);
      owl_vk_renderer_resize (renderer, w, h);
      continue;
    }

    owl_model_anim_update (model, renderer->frame,
                           time_stamp - prev_time_stamp, 0);

    owl_vk_renderer_pipeline_bind (renderer, OWL_PIPELINE_ID_MODEL);
    owl_vk_renderer_draw_model (renderer, model, matrix);

    owl_vk_renderer_pipeline_bind (renderer, OWL_PIPELINE_ID_TEXT);
    owl_ui_renderer_stats_draw (renderer);

    code = owl_vk_renderer_frame_end (renderer);
    if (OWL_ERROR_OUTDATED_SWAPCHAIN == code) {
      owl_i32 w, h;
      owl_window_get_framebuffer_size (window, &w, &h);
      owl_vk_renderer_resize (renderer, w, h);
      continue;
    }

    owl_window_poll_events (window);
  }

  owl_vk_font_deinit (font, &renderer->context);
  free (font);

  owl_model_deinit (model, renderer);
  free (model);

  owl_vk_renderer_deinit (renderer);
  free (renderer);

  owl_window_deinit (window);
  free (window);
}

#include <owl/owl.h>

#include <stdio.h>

#define MODELPATH "../../assets/Suzanne.gltf"
#define FONTPATH "../../assets/SourceCodePro-Regular.ttf"
#define TEXPATH "../../assets/Chaeyoung.jpeg"

#define TEST(fn)                                                             \
  do {                                                                       \
    enum owl_code err = (fn);                                                \
    if (OWL_SUCCESS != (err)) {                                              \
      printf("something went wrong in call: %s, err %i\n", (#fn), err);      \
      return 0;                                                              \
    }                                                                        \
  } while (0)

char const *fps_string(double time) {
  static char buffer[256];
  snprintf(buffer, 256, "fps: %.2f\n", 1 / time);
  return buffer;
}

static struct owl_window_info window_info;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_font *font;
static struct owl_input_state *input;
static struct owl_text_cmd text;
static struct owl_model *model;
static struct owl_draw_cmd_ubo ubo;

int main(void) {
  owl_v3 eye, center, up, pos;

  window_info.height = 600;
  window_info.width = 600;
  window_info.title = "model";

  TEST(owl_window_create(&window_info, &input, &window));
  TEST(owl_renderer_create(window, &renderer));
  TEST(owl_font_create(renderer, 64, FONTPATH, &font));
  TEST(owl_model_create_from_file(renderer, MODELPATH, &model));

  text.font = font;
  OWL_SET_V3(1.0F, 1.0F, 1.0F, text.color);
  OWL_SET_V2(-1.0F, -0.93F, text.pos);

  OWL_IDENTITY_M4(ubo.model);
  OWL_IDENTITY_M4(ubo.view);
  OWL_IDENTITY_M4(ubo.projection);

#if 1
  owl_perspective_m4(OWL_DEG_TO_RAD(45.0F), 1.0F, 0.01F, 100.0F, ubo.projection);

  OWL_SET_V3(0.0F, 0.0F, 5.0F, eye);
  OWL_SET_V3(0.0F, 0.0F, 0.0F, center);
  OWL_SET_V3(0.0F, 1.0F, 0.0F, up);
  owl_look_at_m4(eye, center, up, ubo.view);

  OWL_SET_V3(0.0F, 0.0F, -1.0F, pos);
  owl_translate_m4(pos, ubo.model);
#endif

  while (!owl_window_is_done(window)) {
    if (OWL_SUCCESS != owl_frame_begin(renderer)) {
      owl_renderer_recreate_swapchain(window, renderer);
      continue;
    }

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_PBR);
    owl_model_submit(renderer, &ubo, model);

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
    text.text = fps_string(input->dt_time);
    owl_text_cmd_submit(renderer, &text);

    if (OWL_SUCCESS != owl_frame_end(renderer)) {
      owl_renderer_recreate_swapchain(window, renderer);
      continue;
    }

    owl_window_poll(window);
  }

  owl_model_destroy(renderer, model);
  owl_font_destroy(renderer, font);
  owl_renderer_destroy(renderer);
  owl_window_destroy(window);
}

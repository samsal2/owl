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
  static char buf[256];
  snprintf(buf, 256, "fps: %.2f\n", 1 / time);
  return buf;
}

static struct owl_window_desc window_desc;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_font *font;
static struct owl_input_state *input;
static struct owl_text_cmd text;
static struct owl_model *model;
static struct owl_draw_cmd_ubo ubo;

int main(void) {
  owl_v3 eye, center, up, pos;

  window_desc.height = 600;
  window_desc.width = 600;
  window_desc.title = "model";

  TEST(owl_create_window(&window_desc, &input, &window));
  TEST(owl_create_renderer(window, &renderer));
  TEST(owl_create_font(renderer, 64, FONTPATH, &font));
  TEST(owl_create_model_from_file(renderer, MODELPATH, &model));

  text.font = font;
  OWL_SET_V3(1.0F, 1.0F, 1.0F, text.color);
  OWL_SET_V2(-1.0F, -0.93F, text.pos);

  OWL_IDENTITY_M4(ubo.model);
  OWL_IDENTITY_M4(ubo.view);
  OWL_IDENTITY_M4(ubo.proj);

#if 1
  owl_perspective_m4(OWL_DEG_TO_RAD(45.0F), 1.0F, 0.01F, 100.0F, ubo.proj);

  OWL_SET_V3(0.0F, 0.0F, 5.0F, eye);
  OWL_SET_V3(0.0F, 0.0F, 0.0F, center);
  OWL_SET_V3(0.0F, 1.0F, 0.0F, up);
  owl_look_at_m4(eye, center, up, ubo.view);

  OWL_SET_V3(0.0F, 0.0F, -1.0F, pos);
  owl_translate_m4(pos, ubo.model);
#endif

  while (!owl_is_window_done(window)) {
    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_PBR);
    owl_submit_model(renderer, &ubo, model);

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
    text.text = fps_string(input->dt_time);
    owl_submit_text_cmd(renderer, &text);

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_poll_window_input(window);
  }

  owl_destroy_model(renderer, model);
  owl_destroy_font(renderer, font);
  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}

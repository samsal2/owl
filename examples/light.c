#include "owl/fwd.h"
#include "owl/pipelines.h"
#include "owl/render_group.h"
#include <owl/owl.h>

#include <stdio.h>

static char const *fps_string(OwlSeconds time) {
  static char buffer[256];
  snprintf(buffer, 256, "fps: %.2f\n", 1 / time);
  return buffer;
}

static void fill_group(OwlTexture texture, struct owl_render_group *group) {
  OwlV3 eye;
  OwlV3 center;
  OwlV3 up;

  struct owl_vertex *v;

  group->type = OWL_RENDER_GROUP_LIGHT_TEST;
  group->storage.as_light.texture = texture;

  OWL_IDENTITY_M4(group->storage.as_light.pvm.model);
  OWL_IDENTITY_M4(group->storage.as_light.pvm.view);
  OWL_IDENTITY_M4(group->storage.as_light.pvm.proj);

  owl_perspective_m4(OWL_DEG_TO_RAD(90.0F), 1.0F, 0.01F, 10.0F,
                     group->storage.as_light.pvm.proj);

  OWL_SET_V3(0.0F, 0.0F, 2.0F, eye);
  OWL_SET_V3(0.0F, 0.0F, 0.0F, center);
  OWL_SET_V3(0.0F, 1.0F, 0.0F, up);

  owl_look_at_m4(eye, center, up, group->storage.as_light.pvm.view);

  v = &group->storage.as_light.vertex[0];
  OWL_SET_V3(-0.5F, -0.5F, -1.0F, v->pos);
  OWL_SET_V3(0.0F, 0.0F, 1.0F, v->color); /* FIXME: dif struct for normals */
  OWL_SET_V2(0.0F, 0.0F, v->uv);

  v = &group->storage.as_light.vertex[1];
  OWL_SET_V3(0.5F, -0.5F, -1.0F, v->pos);
  OWL_SET_V3(0.0F, 0.0F, 1.0F, v->color); /* FIXME: dif struct for normals */
  OWL_SET_V2(1.0F, 0.0F, v->uv);

  v = &group->storage.as_light.vertex[2];
  OWL_SET_V3(-0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(0.0F, 0.0F, 1.0F, v->color); /* FIXME: dif struct for normals */
  OWL_SET_V2(0.0F, 1.0F, v->uv);

  v = &group->storage.as_light.vertex[3];
  OWL_SET_V3(0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(0.0F, 0.0F, 1.0F, v->color); /* FIXME: dif struct for normals */
  OWL_SET_V2(1.0F, 1.0F, v->uv);

  OWL_ZERO_V3(group->storage.as_light.lighting.light);
  OWL_SET_V3(0.0F, 0.0F, 1.0F, group->storage.as_light.lighting.view);
}

#define TEST(fn)                                                             \
  do {                                                                       \
    if (OWL_SUCCESS != (fn)) {                                               \
      printf("something went wrong in call: %s\n", (#fn));                   \
      return 0;                                                              \
    }                                                                        \
  } while (0)

#define TEXPATH "../../assets/Chaeyoung.jpeg"
#define FONTPATH "../../assets/Anonymous Pro.ttf"

static struct owl_window *window;
static struct owl_renderer *renderer;
static OwlTexture texture;
static struct owl_render_group group;
static struct owl_font *font;

int main(void) {
  OwlSeconds frame;
  OwlV2 fpspos;
  OwlV3 fpscolor;

  TEST(owl_create_window(600, 600, "light", &window));
  TEST(owl_create_renderer(window, &renderer));
  TEST(owl_create_texture_from_file(renderer, TEXPATH,
                                    OWL_SAMPLER_TYPE_LINEAR, &texture));
  TEST(owl_create_font(renderer, 64, FONTPATH, &font));

  OWL_SET_V2(-1.0F, -0.93F, fpspos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, fpscolor);

  fill_group(texture, &group);

  while (!owl_should_window_close(window)) {
    struct owl_timer timer;

    owl_start_timer(&timer);

    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      owl_recreate_renderer(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_LIGHT);
    owl_submit_render_group(renderer, &group);

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
    owl_submit_text_group(renderer, font, fpspos, fpscolor,
                          fps_string(frame));

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      owl_recreate_renderer(window, renderer);
      continue;
    }

    owl_poll_events(window);
    frame = owl_end_timer(&timer);
  }

  owl_destroy_font(renderer, font);
  owl_destroy_texture(renderer, texture);
  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}

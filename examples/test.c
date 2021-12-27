#include <owl/owl.h>
#include <stdio.h>

void fill_quad(struct owl_render_group *group, OwlTexture texture) {
  struct owl_vertex *v;

  group->type = OWL_RENDER_GROUP_TYPE_QUAD;
  group->storage.as_quad.texture = texture;

  v = &group->storage.as_quad.vertex[0];
  OWL_SET_V3(-0.5F, -0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertex[1];
  OWL_SET_V3(0.5F, -0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertex[2];
  OWL_SET_V3(-0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 1.0F, v->uv);

  v = &group->storage.as_quad.vertex[3];
  OWL_SET_V3(0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 1.0F, v->uv);

  OWL_IDENTITY_M4(group->storage.as_quad.pvm.proj);
  OWL_IDENTITY_M4(group->storage.as_quad.pvm.view);
  OWL_IDENTITY_M4(group->storage.as_quad.pvm.model);
}

static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static OwlTexture texture;
static struct owl_render_group group;

#define TEST(fn)                                                             \
  do {                                                                       \
    if (OWL_SUCCESS != (fn))                                                 \
      printf("error at: %s\n", #fn);                                         \
  } while (0)

#define TEXPATH "../../assets/Chaeyoung.jpeg"

int main(void) {
  TEST(owl_create_window(600, 600, "OWL", &window));
  TEST(owl_create_renderer(window, &renderer));
  TEST(owl_create_texture_from_file(renderer, TEXPATH,
                                    OWL_SAMPLER_TYPE_LINEAR, &texture));

  fill_quad(&group, texture);

  while (!owl_should_window_close(window)) {
    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      printf("recreating renderer\n");
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MAIN);
    owl_submit_render_group(renderer, &group);

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      printf("recreating renderer\n");
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_poll_events(window);
  }

  owl_destroy_texture(renderer, texture);
  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}

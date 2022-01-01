#include <owl/owl.h>
#include <stdio.h>

void fill_quad(struct owl_draw_cmd *group, struct owl_img *img) {
  struct owl_draw_cmd_vertex *v;

  group->type = OWL_DRAW_CMD_TYPE_QUAD;
  group->storage.as_quad.img = img;

  v = &group->storage.as_quad.vertices[0];
  OWL_SET_V3(-0.5F, -0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertices[1];
  OWL_SET_V3(0.5F, -0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertices[2];
  OWL_SET_V3(-0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 1.0F, v->uv);

  v = &group->storage.as_quad.vertices[3];
  OWL_SET_V3(0.5F, 0.5F, 0.0F, v->pos);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 1.0F, v->uv);

  OWL_IDENTITY_M4(group->storage.as_quad.ubo.proj);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.view);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.model);
}

static struct owl_window_desc window_desc;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_img_desc img_desc;
static struct owl_img *img;
static struct owl_draw_cmd group;
static struct owl_input_state *input;

#define TEST(fn)                                                               \
  do {                                                                         \
    if (OWL_SUCCESS != (fn))                                                   \
      printf("error at: %s\n", #fn);                                           \
  } while (0)

#define TEXPATH "../../assets/Chaeyoung.jpeg"

int main(void) {
  window_desc.height = 600;
  window_desc.width = 600;
  window_desc.title = "test";

  TEST(owl_create_window(&window_desc, &input, &window));
  TEST(owl_create_renderer(window, &renderer));

  img_desc.mip_mode = OWL_SAMPLER_MIP_MODE_LINEAR;
  img_desc.min_filter = OWL_SAMPLER_FILTER_LINEAR;
  img_desc.mag_filter = OWL_SAMPLER_FILTER_LINEAR;
  img_desc.wrap_u = OWL_SAMPLER_ADDR_MODE_REPEAT;
  img_desc.wrap_v = OWL_SAMPLER_ADDR_MODE_REPEAT;
  img_desc.wrap_w = OWL_SAMPLER_ADDR_MODE_REPEAT;

  TEST(owl_create_img_from_file(renderer, &img_desc, TEXPATH, &img));

  fill_quad(&group, img);

  while (!owl_is_window_done(window)) {

    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      printf("recreating renderer\n");
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MAIN);
    owl_submit_draw_cmd(renderer, &group);

    if (OWL_SUCCESS != owl_end_frame(renderer)) {
      printf("recreating renderer\n");
      owl_recreate_swapchain(window, renderer);
      continue;
    }

    owl_poll_window_input(window);
  }

  owl_destroy_img(renderer, img);
  owl_destroy_renderer(renderer);
  owl_destroy_window(window);
}

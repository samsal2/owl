#include <owl/owl.h>
#include <stdio.h>

void fill_quad(struct owl_draw_cmd *group, struct owl_texture *texture) {
  struct owl_draw_cmd_vertex *v;

  group->type = OWL_DRAW_CMD_TYPE_QUAD;
  group->storage.as_quad.texture = texture;

  v = &group->storage.as_quad.vertices[0];
  OWL_SET_V3(-0.5F, -0.5F, 0.0F, v->position);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertices[1];
  OWL_SET_V3(0.5F, -0.5F, 0.0F, v->position);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 0.0F, v->uv);

  v = &group->storage.as_quad.vertices[2];
  OWL_SET_V3(-0.5F, 0.5F, 0.0F, v->position);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(0.0F, 1.0F, v->uv);

  v = &group->storage.as_quad.vertices[3];
  OWL_SET_V3(0.5F, 0.5F, 0.0F, v->position);
  OWL_SET_V3(1.0F, 1.0F, 1.0F, v->color);
  OWL_SET_V2(1.0F, 1.0F, v->uv);

  OWL_IDENTITY_M4(group->storage.as_quad.ubo.projection);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.view);
  OWL_IDENTITY_M4(group->storage.as_quad.ubo.model);
}

static struct owl_window_desc window_desc;
static struct owl_window *window;
static struct owl_vk_renderer *renderer;
static struct owl_texture_desc tex_desc;
static struct owl_texture *texture;
static struct owl_draw_cmd group;
static struct owl_input_state *input;

#define TEST(fn)                                                             \
  do {                                                                       \
    if (OWL_SUCCESS != (fn))                                                 \
      printf("error at: %s\n", #fn);                                         \
  } while (0)

#define TEXPATH "../../assets/Chaeyoung.jpeg"

int main(void) {
  window_desc.height = 600;
  window_desc.width = 600;
  window_desc.title = "test";

  TEST(owl_window_create(&window_desc, &input, &window));
  TEST(owl_renderer_create(window, &renderer));

  tex_desc.mip_mode = OWL_SAMPLER_MIP_MODE_LINEAR;
  tex_desc.min_filter = OWL_SAMPLER_FILTER_LINEAR;
  tex_desc.mag_filter = OWL_SAMPLER_FILTER_LINEAR;
  tex_desc.wrap_u = OWL_SAMPLER_ADDR_MODE_REPEAT;
  tex_desc.wrap_v = OWL_SAMPLER_ADDR_MODE_REPEAT;
  tex_desc.wrap_w = OWL_SAMPLER_ADDR_MODE_REPEAT;

  TEST(owl_texture_create_from_file(renderer, &tex_desc, TEXPATH, &texture));

  fill_quad(&group, texture);

  while (!owl_window_is_done(window)) {

    if (OWL_SUCCESS != owl_frame_begin(renderer)) {
      printf("recreating renderer\n");
      owl_renderer_recreate_swapchain(window, renderer);
      continue;
    }

    owl_renderer_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MAIN);
    owl_draw_cmd_submit(renderer, &group);

    if (OWL_SUCCESS != owl_frame_end(renderer)) {
      printf("recreating renderer\n");
      owl_renderer_recreate_swapchain(window, renderer);
      continue;
    }

    owl_window_poll(window);
  }

  owl_texture_destroy(renderer, texture);
  owl_renderer_destroy(renderer);
  owl_window_destroy(window);
}

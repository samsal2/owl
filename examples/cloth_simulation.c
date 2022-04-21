#include <owl.h>

#include <stdio.h>

#define CLOTH_H 32
#define CLOTH_W 32
#define PARTICLE_COUNT (CLOTH_W * CLOTH_H)
#define IDXS_H (CLOTH_H - 1)
#define IDXS_W (CLOTH_W - 1)
#define IDXS_COUNT (IDXS_H * IDXS_W * 6)
#define DAMPING 0.002F
#define STEPS 4
#define FRICTION 0.5F
#define GRAVITY 0.05

#define CLOTH_COORD(i, j) ((i)*CLOTH_W + (j))

struct particle {
  int movable;
  owl_v3 position;
  owl_v3 velocity;
  owl_v3 acceleration;
  owl_v3 previous_position;
  float rest_distances[4];
  struct particle *links[4];
};

struct cloth {
  struct particle particles[PARTICLE_COUNT];

  /* priv draw desc */
  struct owl_draw_command_basic command_;
  owl_u32 indices_[IDXS_COUNT];
  struct owl_draw_command_vertex vertices_[PARTICLE_COUNT];
};

static void init_cloth_(struct cloth *cloth) {
  owl_u32 i, j;

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD(i, j);
      float const w = 2.0F * (float)j / (float)(CLOTH_W - 1) - 1.0F;
      float const h = 2.0F * (float)i / (float)(CLOTH_H - 1) - 1.0F;
      struct particle *p = &cloth->particles[k];

      p->movable = 1;

      OWL_V3_SET(w, h, 0.0F, p->position);
      OWL_V3_SET(0.0F, 0.0F, 0.0F, p->velocity);
      OWL_V3_SET(0.0F, GRAVITY, 0.8F, p->acceleration);
      OWL_V3_SET(w, h, 0.0F, p->previous_position);

      OWL_V3_SET(w, h, 0.0F, cloth->vertices_[k].position);
      OWL_V3_SET(1.0F, 1.0F, 1.0F, cloth->vertices_[k].color);
      OWL_V2_SET((w + 1.0F) / 2.0F, (h + 1.0F) / 2.0F, cloth->vertices_[k].uv);
    }
  }

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD(i, j);
      struct particle *p = &cloth->particles[k];

      if (j) {
        owl_u32 link = CLOTH_COORD(i, j - 1);
        p->links[0] = &cloth->particles[link];
        p->rest_distances[0] =
            owl_v3_distance(p->position, p->links[0]->position);
      } else {
        p->links[0] = NULL;
        p->rest_distances[0] = 0.0F;
      }

      if (i) {
        owl_u32 link = CLOTH_COORD(i - 1, j);
        p->links[1] = &cloth->particles[link];
        p->rest_distances[1] =
            owl_v3_distance(p->position, p->links[1]->position);
      } else {
        p->links[1] = NULL;
        p->rest_distances[1] = 0.0F;
      }

      if (j < CLOTH_H - 1) {
        owl_u32 link = CLOTH_COORD(i, j + 1);
        p->links[2] = &cloth->particles[link];
        p->rest_distances[2] =
            owl_v3_distance(p->position, p->links[2]->position);
      } else {
        p->links[2] = NULL;
        p->rest_distances[2] = 0.0F;
      }

      if (i < CLOTH_W - 1) {
        owl_u32 link = CLOTH_COORD(i + 1, j);
        p->links[3] = &cloth->particles[link];
        p->rest_distances[3] =
            owl_v3_distance(p->position, p->links[3]->position);
      } else {
        p->links[3] = NULL;
        p->rest_distances[3] = 0.0F;
      }
    }
  }

  for (i = 0; i < IDXS_H; ++i) {
    for (j = 0; j < IDXS_W; ++j) {
      owl_u32 const k = i * CLOTH_W + j;
      owl_u32 const fix_k = (i * IDXS_W + j) * 6;

      cloth->indices_[fix_k + 0] = k + CLOTH_W;
      cloth->indices_[fix_k + 1] = k + CLOTH_W + 1;
      cloth->indices_[fix_k + 2] = k + 1;
      cloth->indices_[fix_k + 3] = k + 1;
      cloth->indices_[fix_k + 4] = k;
      cloth->indices_[fix_k + 5] = k + CLOTH_W;
    }
  }

  for (i = 0; i < CLOTH_W; ++i)
    cloth->particles[CLOTH_COORD(0, i)].movable = 0;
}

static void update_cloth(float dt, struct cloth *cloth) {
  owl_u32 i;
  for (i = 0; i < PARTICLE_COUNT; ++i) {
    owl_u32 j;
    owl_v3 tmp;
    struct particle *p = &cloth->particles[i];

    if (!p->movable)
      continue;

    p->acceleration[1] += GRAVITY;

    OWL_V3_SUB(p->position, p->previous_position, p->velocity);
    OWL_V3_SCALE(p->velocity, 1.0F - DAMPING, p->velocity);
    OWL_V3_SCALE(p->acceleration, dt, tmp);
    OWL_V3_ADD(tmp, p->velocity, tmp);
    OWL_V3_COPY(p->position, p->previous_position);
    OWL_V3_ADD(p->position, tmp, p->position);
    OWL_V3_ZERO(p->acceleration);

    /* constraint */
    for (j = 0; j < STEPS; ++j) {
      owl_u32 k;
      for (k = 0; k < 4; ++k) {
        float factor;
        owl_v3 delta;
        owl_v3 correction;
        struct particle *link = p->links[k];

        if (!link)
          continue;

        OWL_V3_SUB(link->position, p->position, delta);
        factor = 1 - (p->rest_distances[k] / owl_v3_magnitude(delta));
        OWL_V3_SCALE(delta, factor, correction);
        OWL_V3_SCALE(correction, 0.5F, correction);
        OWL_V3_ADD(p->position, correction, p->position);

        if (!link->movable)
          continue;

        OWL_V3_NEGATE(correction, correction);
        OWL_V3_ADD(link->position, correction, link->position);
      }
    }
  }

  for (i = 0; i < PARTICLE_COUNT; ++i)
    OWL_V3_COPY(cloth->particles[i].position, cloth->vertices_[i].position);
}

static void change_particle_position(owl_u32 id, owl_v2 const position,
                                     struct cloth *cloth) {
  struct particle *p = &cloth->particles[id];

  if (p->movable)
    OWL_V2_COPY(position, p->position);
}

static owl_u32 select_particle_at(owl_v2 const pos, struct cloth *cloth) {
  owl_u32 i;
  owl_u32 current = 0;
  float min_dist = owl_v2_distance(pos, cloth->particles[current].position);

  for (i = 1; i < PARTICLE_COUNT; ++i) {
    struct particle *p = &cloth->particles[i];
    float const new_dist = owl_v2_distance(pos, p->position);

    if (new_dist < min_dist) {
      current = i;
      min_dist = new_dist;
    }
  }

  return current;
}

void init_cloth(struct cloth *cloth, struct owl_renderer_image *image) {
  owl_v3 position;

  init_cloth_(cloth);

  cloth->command_.image.slot = image->slot;
  cloth->command_.indices_count = IDXS_COUNT;
  cloth->command_.indices = cloth->indices_;
  cloth->command_.vertices_count = PARTICLE_COUNT;
  cloth->command_.vertices = cloth->vertices_;

  OWL_V3_SET(0.0F, 0.0F, -2.0F, position);
  OWL_M4_IDENTITY(cloth->command_.model);
  owl_m4_translate(position, cloth->command_.model);
}

char const *fps_string(double time) {
  static char buffer[256];
  snprintf(buffer, 256, "fps: %.2f\n", 1 / time);
  return buffer;
}

#define TEST(fn)                                                               \
  do {                                                                         \
    enum owl_code code = (fn);                                                 \
    if (OWL_SUCCESS != (code)) {                                               \
      printf("something went wrong in call: %s, code %i\n", (#fn), code);      \
      return 0;                                                                \
    }                                                                          \
  } while (0)

static struct owl_client_init_desc client_init_desc;
static struct owl_client *client;
static struct owl_renderer_init_desc renderer_init_desc;
static struct owl_renderer *renderer;
static struct owl_renderer_image_init_desc image_init_desc;
static struct owl_renderer_image image;
static struct cloth cloth;
static struct owl_font font;
static struct owl_draw_command_text text_command;
static struct owl_camera camera;

#define UNSELECTED (owl_u32) - 1
#define TPATH "../assets/cloth.jpeg"
#define FONTPATH "../assets/Inconsolata-Regular.ttf"

int main(void) {
  owl_u32 selected = UNSELECTED;

  client_init_desc.height = 600;
  client_init_desc.width = 600;
  client_init_desc.title = "cloth-sim";
  client = OWL_MALLOC(sizeof(*client));
  TEST(owl_client_init(&client_init_desc, client));

  TEST(owl_client_fill_renderer_init_desc(client, &renderer_init_desc));
  renderer = OWL_MALLOC(sizeof(*renderer));
  TEST(owl_renderer_init(&renderer_init_desc, renderer));

  image_init_desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_FILE;
  image_init_desc.src_path = TPATH;
  image_init_desc.use_default_sampler = 1;
  TEST(owl_renderer_image_init(renderer, &image_init_desc, &image));

  TEST(owl_font_init(renderer, 64, FONTPATH, &font));

  TEST(owl_camera_init(&camera));

  init_cloth(&cloth, &image);

  text_command.font = &font;
  OWL_V3_SET(1.0F, 1.0F, 1.0F, text_command.color);
  OWL_V3_SET(-0.5F, -0.5F, -1.0F, text_command.position);

  while (!owl_client_is_done(client)) {
#if 1

    if (UNSELECTED == selected &&
        OWL_BUTTON_STATE_PRESS == client->mouse_buttons[OWL_MOUSE_BUTTON_LEFT])
      selected = select_particle_at(client->cursor_position, &cloth);

    if (UNSELECTED != selected)
      change_particle_position(selected, client->cursor_position, &cloth);

    if (UNSELECTED != selected &&
        OWL_BUTTON_STATE_RELEASE ==
            client->mouse_buttons[OWL_MOUSE_BUTTON_LEFT])
      selected = UNSELECTED;

#endif

    update_cloth(1.0F / 60.0F, &cloth);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_begin_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_init_desc);
      owl_renderer_resize_swapchain(&renderer_init_desc, renderer);
      continue;
    }

#if 1
    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_TYPE_MAIN);
#else
    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_TYPE_WIRES);
#endif
    owl_draw_command_submit_basic(renderer, &camera, &cloth.command_);

    owl_renderer_bind_pipeline(renderer, OWL_RENDERER_PIPELINE_TYPE_FONT);
    text_command.text = fps_string(client->delta_time_stamp);
    owl_draw_command_submit_text(renderer, &camera, &text_command);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_end_frame(renderer)) {
      owl_client_fill_renderer_init_desc(client, &renderer_init_desc);
      owl_renderer_resize_swapchain(&renderer_init_desc, renderer);
      continue;
    }

    owl_client_poll_events(client);
  }

  owl_font_deinit(renderer, &font);
  owl_renderer_image_deinit(renderer, &image);
  owl_renderer_deinit(renderer);
  owl_client_deinit(client);
}

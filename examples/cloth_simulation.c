#include <owl/owl.h>

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
  OwlV3 pos;
  OwlV3 velocity;
  OwlV3 acceleration;
  OwlV3 previous_position;
  float rest_distances[4];
  struct particle *links[4];
};

struct cloth {
  struct particle particles[PARTICLE_COUNT];

  /* priv draw info */
  struct owl_render_group group;

  OwlU32 indices_[IDXS_COUNT];
  struct owl_vertex vertices_[PARTICLE_COUNT];
};

static void init_cloth_(struct cloth *cloth) {
  OwlU32 i, j;

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      OwlU32 const k = CLOTH_COORD(i, j);
      float const w = 2.0F * (float)j / (float)(CLOTH_W - 1) - 1.0F;
      float const h = 2.0F * (float)i / (float)(CLOTH_H - 1) - 1.0F;
      struct particle *p = &cloth->particles[k];

      p->movable = 1;

      OWL_SET_V3(w, h, 0.0F, p->pos);
      OWL_SET_V3(0.0F, 0.0F, 0.0F, p->velocity);
      OWL_SET_V3(0.0F, GRAVITY, 0.8F, p->acceleration);
      OWL_SET_V3(w, h, 0.0F, p->previous_position);

      OWL_SET_V3(w, h, 0.0F, cloth->vertices_[k].pos);
      OWL_SET_V3(1.0F, 1.0F, 1.0F, cloth->vertices_[k].color);
      OWL_SET_V2((w + 1.0F) / 2.0F, (h + 1.0F) / 2.0F,
                 cloth->vertices_[k].uv);
    }
  }

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      OwlU32 const k = CLOTH_COORD(i, j);
      struct particle *p = &cloth->particles[k];

      if (j) {
        OwlU32 link = CLOTH_COORD(i, j - 1);
        p->links[0] = &cloth->particles[link];
        p->rest_distances[0] = owl_dist_v3(p->pos, p->links[0]->pos);
      } else {
        p->links[0] = NULL;
        p->rest_distances[0] = 0.0F;
      }

      if (i) {
        OwlU32 link = CLOTH_COORD(i - 1, j);
        p->links[1] = &cloth->particles[link];
        p->rest_distances[1] = owl_dist_v3(p->pos, p->links[1]->pos);
      } else {
        p->links[1] = NULL;
        p->rest_distances[1] = 0.0F;
      }

      if (j < CLOTH_H - 1) {
        OwlU32 link = CLOTH_COORD(i, j + 1);
        p->links[2] = &cloth->particles[link];
        p->rest_distances[2] = owl_dist_v3(p->pos, p->links[2]->pos);
      } else {
        p->links[2] = NULL;
        p->rest_distances[2] = 0.0F;
      }

      if (i < CLOTH_W - 1) {
        OwlU32 link = CLOTH_COORD(i + 1, j);
        p->links[3] = &cloth->particles[link];
        p->rest_distances[3] = owl_dist_v3(p->pos, p->links[3]->pos);
      } else {
        p->links[3] = NULL;
        p->rest_distances[3] = 0.0F;
      }
    }
  }

  for (i = 0; i < IDXS_H; ++i) {
    for (j = 0; j < IDXS_W; ++j) {
      OwlU32 const k = i * CLOTH_W + j;
      OwlU32 const fix_k = (i * IDXS_W + j) * 6;

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
  OwlU32 i;
  for (i = 0; i < PARTICLE_COUNT; ++i) {
    OwlU32 j;
    OwlV3 tmp;
    struct particle *p = &cloth->particles[i];

    if (!p->movable)
      continue;

    p->acceleration[1] += GRAVITY;

    OWL_SUB_V3(p->pos, p->previous_position, p->velocity);
    OWL_SCALE_V3(p->velocity, 1.0F - DAMPING, p->velocity);
    OWL_SCALE_V3(p->acceleration, dt, tmp);
    OWL_ADD_V3(tmp, p->velocity, tmp);
    OWL_COPY_V3(p->pos, p->previous_position);
    OWL_ADD_V3(p->pos, tmp, p->pos);
    OWL_ZERO_V3(p->acceleration);

    /* constraint */
    for (j = 0; j < STEPS; ++j) {
      OwlU32 k;
      for (k = 0; k < 4; ++k) {
        float factor;
        OwlV3 delta;
        OwlV3 correction;
        struct particle *link = p->links[k];

        if (!link)
          continue;

        OWL_SUB_V3(link->pos, p->pos, delta);
        factor = 1 - (p->rest_distances[k] / owl_mag_v3(delta));
        OWL_SCALE_V3(delta, factor, correction);
        OWL_SCALE_V3(correction, 0.5F, correction);
        OWL_ADD_V3(p->pos, correction, p->pos);

        if (!link->movable)
          continue;

        OWL_NEGATE_V3(correction, correction);
        OWL_ADD_V3(link->pos, correction, link->pos);
      }
    }
  }

  for (i = 0; i < PARTICLE_COUNT; ++i)
    OWL_COPY_V3(cloth->particles[i].pos, cloth->vertices_[i].pos);
}

static void change_particle_position(OwlU32 id, OwlV2 const position,
                                     struct cloth *cloth) {
  struct particle *p = &cloth->particles[id];

  if (p->movable)
    OWL_COPY_V2(position, p->pos);
}

static OwlU32 select_particle_at(OwlV2 const pos, struct cloth *cloth) {
  OwlU32 i;
  OwlU32 current = 0;
  float min_dist = owl_dist_v2(pos, cloth->particles[current].pos);

  for (i = 1; i < PARTICLE_COUNT; ++i) {
    struct particle *p = &cloth->particles[i];
    float const new_dist = owl_dist_v2(pos, p->pos);

    if (new_dist < min_dist) {
      current = i;
      min_dist = new_dist;
    }
  }

  return current;
}

void init_cloth(struct cloth *cloth, OwlTexture texture) {
  init_cloth_(cloth);

  cloth->group.type = OWL_RENDER_GROUP_TYPE_BASIC;
  cloth->group.storage.as_basic.texture = texture;
  cloth->group.storage.as_basic.index_count = IDXS_COUNT;
  cloth->group.storage.as_basic.index = cloth->indices_;
  cloth->group.storage.as_basic.vertex_count = PARTICLE_COUNT;
  cloth->group.storage.as_basic.vertex = cloth->vertices_;
}

struct owl_uniform *get_cloth_pvm(struct cloth *cloth) {
  return &cloth->group.storage.as_basic.pvm;
}

char const *fps_string(OwlSeconds time) {
  static char buf[256];
  snprintf(buf, 256, "fps: %.2f\n", 1 / time);
  return buf;
}

#define TEST(fn)                                                             \
  do {                                                                       \
    if (OWL_SUCCESS != (fn)) {                                               \
      printf("something went wrong in call: %s\n", (#fn));                   \
      return 0;                                                              \
    }                                                                        \
  } while (0)

static struct owl_window *window;
static struct owl_renderer *renderer;
static OwlTexture texture;
static struct owl_render_group group;
static struct cloth cloth;
static struct owl_font *font;

#define UNSELECTED (OwlU32) - 1
#define TEXPATH "../../assets/Chaeyoung.jpeg"
#define FONTPATH "../../assets/Anonymous Pro.ttf"

int main(void) {
  OwlV2 fpspos;
  OwlV3 eye;
  OwlV3 center;
  OwlV3 up;
  OwlV3 position;
  OwlV3 color;
  struct owl_uniform *pvm;
  OwlU32 selected = UNSELECTED;
  float frame = 0.0F;

  TEST(owl_create_window(600, 600, "cloth-simulation", &window));
  TEST(owl_create_renderer(window, &renderer));
  TEST(owl_create_texture_from_file(renderer, TEXPATH,
                                    OWL_SAMPLER_TYPE_LINEAR, &texture));
  TEST(owl_create_font(renderer, 64, FONTPATH, &font));

  init_cloth(&cloth, texture);

  OWL_SET_V3(1.0F, 1.0F, 1.0F, color);
  OWL_SET_V2(-1.0F, -0.93F, fpspos);

  pvm = get_cloth_pvm(&cloth);

  OWL_IDENTITY_M4(pvm->model);
  OWL_IDENTITY_M4(pvm->view);
  OWL_IDENTITY_M4(pvm->proj);

#if 0
  owl_ortho_m4(-2.0F, 2.0F, -2.0F, 2.0F, 0.1F, 10.0F, pvm->proj);
#else
  owl_perspective_m4(OWL_DEG_TO_RAD(45.0F), 1.0F, 0.01F, 10.0F, pvm->proj);
#endif

  OWL_SET_V3(0.0F, 0.0F, 2.0F, eye);
  OWL_SET_V3(0.0F, 0.0F, 0.0F, center);
  OWL_SET_V3(0.0F, 1.0F, 0.0F, up);

  owl_look_at_m4(eye, center, up, pvm->view);

  OWL_SET_V3(0.0F, 0.0F, -2.0F, position);

  owl_translate_m4(position, pvm->model);

  while (!owl_should_window_close(window)) {
    struct owl_timer timer;

    owl_start_timer(&timer);

#if 1

    if (UNSELECTED == selected &&
        OWL_BUTTON_STATE_PRESS == window->mouse[OWL_MOUSE_BUTTON_LEFT])
      selected = select_particle_at(window->cursor, &cloth);

    if (UNSELECTED != selected)
      change_particle_position(selected, window->cursor, &cloth);

    if (UNSELECTED != selected &&
        OWL_BUTTON_STATE_RELEASE == window->mouse[OWL_MOUSE_BUTTON_LEFT])
      selected = UNSELECTED;

#endif

    update_cloth(1.0F / 60.0F, &cloth);

    if (OWL_SUCCESS != owl_begin_frame(renderer)) {
      owl_recreate_renderer(window, renderer);
      continue;
    }

    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_MAIN);
    owl_submit_render_group(renderer, &cloth.group);

#if 1
    owl_bind_pipeline(renderer, OWL_PIPELINE_TYPE_FONT);
#endif

    owl_submit_text_group(renderer, font, fpspos, color, fps_string(frame));

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

#include "owl/owl_renderer.h"
#include <owl/owl.h>

#include <stdio.h>
#include <stdlib.h>

#define CLOTH_H        32
#define CLOTH_W        32
#define PARTICLE_COUNT (CLOTH_W * CLOTH_H)
#define IDXS_H         (CLOTH_H - 1)
#define IDXS_W         (CLOTH_W - 1)
#define IDX_COUNT      (IDXS_H * IDXS_W * 6)
#define DAMPING        0.002F
#define STEPS          4
#define FRICTION       0.5F
#define GRAVITY        0.05

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
  owl_renderer_image_descriptor image;

  owl_m4 matrix;

  /* priv draw desc */
  owl_u32 indices_[IDX_COUNT];
  struct owl_renderer_vertex vertices_[PARTICLE_COUNT];
};

static void
base_init_cloth (struct cloth *cloth) {
  owl_u32 i, j;

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD (i, j);
      float const w = 2.0F * (float)j / (float)(CLOTH_W - 1) - 1.0F;
      float const h = 2.0F * (float)i / (float)(CLOTH_H - 1) - 1.0F;
      struct particle *p = &cloth->particles[k];

      p->movable = 1;

      owl_v3_set (p->position, w, h, 0.0F);
      owl_v3_set (p->velocity, 0.0F, 0.0F, 0.0F);
      owl_v3_set (p->acceleration, 0.0F, GRAVITY, 0.8F);
      owl_v3_set (p->previous_position, w, h, 0.0F);

      owl_v3_set (cloth->vertices_[k].position, w, h, 0.0F);
      owl_v3_set (cloth->vertices_[k].color, 1.0F, 1.0F, 1.0F);
      owl_v2_set (cloth->vertices_[k].uv, (w + 1.0F) / 2.0F,
                  (h + 1.0F) / 2.0F);
    }
  }

  for (i = 0; i < CLOTH_H; ++i) {
    for (j = 0; j < CLOTH_W; ++j) {
      owl_u32 const k = CLOTH_COORD (i, j);
      struct particle *p = &cloth->particles[k];

      if (j) {
        owl_u32 link = CLOTH_COORD (i, j - 1);
        p->links[0] = &cloth->particles[link];
        p->rest_distances[0] =
            owl_v3_distance (p->position, p->links[0]->position);
      } else {
        p->links[0] = NULL;
        p->rest_distances[0] = 0.0F;
      }

      if (i) {
        owl_u32 link = CLOTH_COORD (i - 1, j);
        p->links[1] = &cloth->particles[link];
        p->rest_distances[1] =
            owl_v3_distance (p->position, p->links[1]->position);
      } else {
        p->links[1] = NULL;
        p->rest_distances[1] = 0.0F;
      }

      if (j < CLOTH_H - 1) {
        owl_u32 link = CLOTH_COORD (i, j + 1);
        p->links[2] = &cloth->particles[link];
        p->rest_distances[2] =
            owl_v3_distance (p->position, p->links[2]->position);
      } else {
        p->links[2] = NULL;
        p->rest_distances[2] = 0.0F;
      }

      if (i < CLOTH_W - 1) {
        owl_u32 link = CLOTH_COORD (i + 1, j);
        p->links[3] = &cloth->particles[link];
        p->rest_distances[3] =
            owl_v3_distance (p->position, p->links[3]->position);
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
    cloth->particles[CLOTH_COORD (0, i)].movable = 0;
}

static void
update_cloth (float dt, struct cloth *cloth) {
  owl_i32 i;
  for (i = 0; i < PARTICLE_COUNT; ++i) {
    owl_u32 j;
    owl_v3 tmp;
    struct particle *p = &cloth->particles[i];

    if (!p->movable)
      continue;

    p->acceleration[1] += GRAVITY;

    owl_v3_sub (p->position, p->previous_position, p->velocity);
    owl_v3_scale (p->velocity, 1.0F - DAMPING, p->velocity);
    owl_v3_scale (p->acceleration, dt, tmp);
    owl_v3_add (tmp, p->velocity, tmp);
    owl_v3_copy (p->position, p->previous_position);
    owl_v3_add (p->position, tmp, p->position);
    owl_v3_zero (p->acceleration);

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

        owl_v3_sub (link->position, p->position, delta);
        factor = 1 - (p->rest_distances[k] / owl_v3_magnitude (delta));
        owl_v3_scale (delta, factor, correction);
        owl_v3_scale (correction, 0.5F, correction);
        owl_v3_add (p->position, correction, p->position);

        if (!link->movable)
          continue;

        owl_v3_negate (correction, correction);
        owl_v3_add (link->position, correction, link->position);
      }
    }
  }

  for (i = 0; i < PARTICLE_COUNT; ++i)
    owl_v3_copy (cloth->particles[i].position, cloth->vertices_[i].position);
}

#if 0
static void change_particle_position(owl_u32 id, owl_v2 const position,
                                     struct cloth *cloth) {
  struct particle *p = &cloth->particles[id];

  if (p->movable)
    owl_v2_copy(position, p->position);
}

static owl_u32 select_particle_at(owl_v2 const pos, struct cloth *cloth) {
  owl_i32 i;
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
#endif

void
init_cloth (struct cloth *cloth, owl_renderer_image_descriptor image) {
  owl_v3 position;

  base_init_cloth (cloth);

  cloth->image = image;

  owl_v3_set (position, 0.0F, 0.0F, -2.0F);
  owl_m4_identity (cloth->matrix);
  owl_m4_translate (position, cloth->matrix);
}

char const *
fps_string (double time) {
  static char buffer[256];
  snprintf (buffer, 256, "fps: %.2f\n", 1 / time);
  return buffer;
}

#define TEST(fn)                                                              \
  do {                                                                        \
    enum owl_code code = (fn);                                                \
    if (OWL_SUCCESS != (code)) {                                              \
      printf ("something went wrong in call: %s, code %i\n", (#fn), code);    \
      return 0;                                                               \
    }                                                                         \
  } while (0)

static struct owl_window *window;
static struct owl_renderer *renderer;
static struct owl_renderer_image_info image_info;
static owl_renderer_image_descriptor image;
static struct cloth cloth;
static struct owl_renderer_vertex_list list;

#define UNSELECTED (owl_u32) - 1
#define TPATH      "../../assets/cloth.jpeg"
#define FONTPATH   "../../assets/Inconsolata-Regular.ttf"

int
main (void) {
  window = malloc (sizeof (*window));
  TEST (owl_window_init (window, 600, 600, "cloth-sim"));

  renderer = malloc (sizeof (*renderer));
  TEST (owl_renderer_init (renderer, window));

  image_info.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_FILE;
  image_info.src_path = TPATH;
  image_info.sampler_use_default = 1;
  TEST (owl_renderer_image_init (renderer, &image_info, &image));

  init_cloth (&cloth, image);

  list.indices = cloth.indices_;
  list.index_count = IDX_COUNT;

  list.vertices = cloth.vertices_;
  list.vertex_count = PARTICLE_COUNT;

  list.texture = cloth.image;

  while (!owl_window_is_done (window)) {
#if 0

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

    update_cloth (1.0F / 60.0F, &cloth);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_begin (renderer)) {
      owl_renderer_swapchain_resize (renderer, window);
      continue;
    }

    owl_renderer_bind_pipeline (renderer, OWL_RENDERER_PIPELINE_MAIN);
    owl_renderer_vertex_list_draw (renderer, &list, cloth.matrix);

    if (OWL_ERROR_OUTDATED_SWAPCHAIN == owl_renderer_frame_end (renderer)) {
      owl_renderer_swapchain_resize (renderer, window);
      continue;
    }

    owl_window_poll_events (window);
  }

  owl_renderer_image_deinit (renderer, image);

  owl_renderer_deinit (renderer);
  free (renderer);

  owl_window_deinit (window);
  free (window);
}

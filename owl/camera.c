#include "camera.h"

#include "internal.h"
#include "vector_math.h"

enum owl_code owl_camera_init(struct owl_camera *c) {
  enum owl_code code = OWL_SUCCESS;

  c->near = 0.01F;
  c->far = 10.0F;
  c->fov = OWL_DEG_TO_RAD(45.0F);
  c->ratio = 1.0F;

#if 0
  owl_m4_ortho(-2.0F, 2.0F, -2.0F, 2.0F, 0.1F, 10.0F, c->projection);
#else
  owl_m4_perspective(c->fov, c->ratio, c->near, c->far, c->projection);
#endif

  OWL_V3_SET(3.0F, 0.0F, 0.0F, c->eye);
  OWL_V3_SET(1.0F, 0.0F, 0.0F, c->direction);
  OWL_V3_SET(0.0F, 1.0F, 0.0F, c->up);
  owl_m4_look(c->eye, c->direction, c->up, c->view);

  return code;
}

void owl_camera_set_direction(struct owl_camera *c, owl_v3 direction) {
  OWL_V3_COPY(direction, c->direction);
  owl_m4_look(c->eye, c->direction, c->up, c->view);
}

void owl_camera_set_pitch_yaw(struct owl_camera *c, float pitch, float yaw) {
  owl_v3_direction(pitch, yaw, c->up, c->direction);
  owl_m4_look(c->eye, c->direction, c->up, c->view);
}

void owl_camera_set_ratio(struct owl_camera *c, float ratio) {
  c->ratio = ratio;
  owl_m4_perspective(c->fov, c->ratio, c->near, c->far, c->projection);
}

void owl_camera_deinit(struct owl_camera *c) { OWL_UNUSED(c); }

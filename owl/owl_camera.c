#include "owl_camera.h"

#include "owl_internal.h"
#include "owl_vector_math.h"

enum owl_code owl_camera_init(struct owl_camera *camera) {
  enum owl_code code = OWL_SUCCESS;

  camera->near = 0.01F;
  camera->far = 10.0F;
  camera->fov = OWL_DEG_AS_RAD(45.0F);
  camera->ratio = 1.0F;

#if 0
  owl_m4_ortho(-2.0F, 2.0F, -2.0F, 2.0F, 0.1F, 10.0F, camera->projection);
#else
  owl_m4_perspective(camera->fov, camera->ratio, camera->near, camera->far,
                     camera->projection);
#endif

  OWL_V3_SET(0.0F, 0.0F, 5.0F, camera->eye);
  OWL_V3_SET(0.0F, 0.0F, 1.0F, camera->direction);
  OWL_V3_SET(0.0F, 1.0F, 0.0F, camera->up);
  owl_m4_look(camera->eye, camera->direction, camera->up, camera->view);

  return code;
}

void owl_camera_set_direction(struct owl_camera *camera, owl_v3 direction) {
  OWL_V3_COPY(direction, camera->direction);
  owl_m4_look(camera->eye, camera->direction, camera->up, camera->view);
}

void owl_camera_set_pitch_yaw(struct owl_camera *camera, float pitch,
                              float yaw) {
  owl_v3_direction(pitch, yaw, camera->up, camera->direction);
  owl_m4_look(camera->eye, camera->direction, camera->up, camera->view);
}

void owl_camera_set_ratio(struct owl_camera *camera, float ratio) {
  camera->ratio = ratio;
  owl_m4_perspective(camera->fov, camera->ratio, camera->near, camera->far,
                     camera->projection);
}

void owl_camera_deinit(struct owl_camera *camera) { OWL_UNUSED(camera); }

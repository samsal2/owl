#include "owl_camera.h"

#include "owl_internal.h"
#include "owl_vector_math.h"

owl_public enum owl_code
owl_camera_init (struct owl_camera *camera, float ratio) {
  float const fov = owl_deg2rad (45.0F);
  float const near = 0.01;
  float const far = 10.0F;

  owl_v3_set (camera->eye, 0.0F, 0.0F, 5.0F);
  owl_v4_set (camera->direction, 0.0F, 0.0F, 1.0F, 1.0F);
  owl_v3_set (camera->up, 0.0F, 1.0F, 0.0F);

  owl_m4_perspective (fov, ratio, near, far, camera->projection);
  owl_m4_look (camera->eye, camera->direction, camera->up, camera->view);

  return OWL_SUCCESS;
}

owl_public void
owl_camera_deinit (struct owl_camera *camera) {
  owl_unused (camera);
}

owl_public void
owl_camera_rotate (struct owl_camera *camera, owl_v3 axis, float angle) {
  owl_m4 rotation;
  owl_m4_make_rotate (angle, axis, rotation);
  owl_m4_multiply (rotation, camera->view, camera->view);
  owl_m4v4_mul (rotation, camera->direction, camera->direction);
}

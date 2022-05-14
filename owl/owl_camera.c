#include "owl_camera.h"

#include "owl_internal.h"
#include "owl_vector_math.h"

owl_public enum owl_code
owl_camera_init (struct owl_camera *camera, float ratio) {
  float fov = owl_deg2rad (45.0F);
  float near = 0.01;
  float far = 10.0F;

  owl_v3 eye = {0.0F, 0.0F, 5.0F};
  owl_v3 direction = {0.0F, 0.0F, 1.0F};
  owl_v3 up = {0.0F, 1.0F, 0.0F};

  owl_m4_perspective (fov, ratio, near, far, camera->projection);
  owl_m4_look (eye, direction, up, camera->view);

  return OWL_SUCCESS;
}

owl_public void
owl_camera_deinit (struct owl_camera *camera) {
  owl_unused (camera);
}

#include "owl_camera.h"

#include "owl_internal.h"
#include "owl_vector_math.h"

owl_public enum owl_code
owl_camera_init (struct owl_camera *camera, float ratio)
{
  float fov = owl_deg2rad (45.0F);
  float near = 0.01;
  float far = 10.0F;

  owl_m4_perspective (fov, ratio, near, far, camera->projection);
  return OWL_SUCCESS;
}

owl_public void
owl_camera_deinit (struct owl_camera *camera)
{
  owl_unused (camera);
}

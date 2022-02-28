#include "camera.h"

#include "vector_math.h"

enum owl_code owl_camera_init(struct owl_camera *cam) {
  enum owl_code code = OWL_SUCCESS;

#if 0
  owl_m4_ortho(-2.0F, 2.0F, -2.0F, 2.0F, 0.1F, 10.0F, pvm->projection);
#else
  owl_m4_perspective(OWL_DEG_TO_RAD(45.0F), 1.0F, 0.01F, 10.0F,
                     cam->projection);
#endif

#if 0
  cam->projection[1][1] *= -1.0F;
#endif

  OWL_V3_SET(0.0F, 0.0F, 3.0F, cam->eye);
  OWL_V3_SET(0.0F, 0.0F, 1.0F, cam->direction);
  OWL_V3_SET(0.0F, 1.0F, 0.0F, cam->up);
  owl_m4_look(cam->eye, cam->direction, cam->up, cam->view);

  return code;
}

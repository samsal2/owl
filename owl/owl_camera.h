#ifndef OWL_CAMERA_H_
#define OWL_CAMERA_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_camera {
  owl_v3 eye;
  owl_v3 up;
  owl_v4 direction;
  owl_m4 projection;
  owl_m4 view;
};

owl_public enum owl_code
owl_camera_init (struct owl_camera *camera, float ratio);

owl_public void
owl_camera_deinit (struct owl_camera *camera);

owl_public void
owl_camera_rotate (struct owl_camera *camera, owl_v3 axis, float angle);

OWL_END_DECLS

#endif

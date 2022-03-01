#ifndef OWL_CAMERA_H_
#define OWL_CAMERA_H_

#include "types.h"

struct owl_camera {
  owl_v3 direction;
  owl_v3 up;
  owl_v3 eye;
  owl_m4 projection;
  owl_m4 view;
};

enum owl_code owl_camera_init(struct owl_camera *c);
void owl_camera_deinit(struct owl_camera *c);

#endif

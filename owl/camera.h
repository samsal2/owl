#ifndef OWL_CAMERA_H_
#define OWL_CAMERA_H_

#include "types.h"

struct owl_camera {
  float near;
  float far;
  float ratio;
  float fov;
  owl_v3 direction;
  owl_v3 up;
  owl_v3 eye;
  owl_m4 projection;
  owl_m4 view;
};

enum owl_code owl_camera_init(struct owl_camera *camera);

void owl_camera_set_direction(struct owl_camera *camera, owl_v3 direction);

void owl_camera_set_pitch_yaw(struct owl_camera *camera, float pitch,
                              float yaw);

void owl_camera_set_ratio(struct owl_camera *camera, float ratio);

void owl_camera_deinit(struct owl_camera *camera);

#endif

#ifndef OWL_CAMERA_H_
#define OWL_CAMERA_H_

#include "owl_types.h"

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

enum owl_code owl_camera_init(struct owl_camera *c);

void owl_camera_direction_set(struct owl_camera *c, const owl_v3 dir);

void owl_camera_set_pitch_yaw(struct owl_camera *c, float pitch, float yaw);

void owl_camera_ratio_set(struct owl_camera *c, float ratio);

void owl_camera_deinit(struct owl_camera *c);

#endif

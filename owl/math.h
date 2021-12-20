#ifndef OWL_MATH_H_
#define OWL_MATH_H_

#include <owl/types.h>

#define OWL_DEG_TO_RAD(angle) ((angle)*0.01745329252F)
#define OWL_RAD_TO_DEG(angle) ((angle)*57.2957795131F)

#define OWL_ZERO_V2(v)                                                       \
  do {                                                                       \
    (v)[0] = 0.0F;                                                           \
    (v)[1] = 0.0F;                                                           \
  } while (0)

#define OWL_ZERO_V3(v)                                                       \
  do {                                                                       \
    (v)[0] = 0.0F;                                                           \
    (v)[1] = 0.0F;                                                           \
    (v)[2] = 0.0F;                                                           \
  } while (0)

#define OWL_ZERO_V4(v)                                                       \
  do {                                                                       \
    (v)[0] = 0.0F;                                                           \
    (v)[1] = 0.0F;                                                           \
    (v)[2] = 0.0F;                                                           \
    (v)[3] = 0.0F;                                                           \
  } while (0)

#define OWL_SET_V2(x, y, out)                                                \
  do {                                                                       \
    (out)[0] = (x);                                                          \
    (out)[1] = (y);                                                          \
  } while (0)

#define OWL_SET_V3(x, y, z, out)                                             \
  do {                                                                       \
    (out)[0] = (x);                                                          \
    (out)[1] = (y);                                                          \
    (out)[2] = (z);                                                          \
  } while (0)

#define OWL_SET_V4(x, y, z, w, out)                                          \
  do {                                                                       \
    (out)[0] = (x);                                                          \
    (out)[1] = (y);                                                          \
    (out)[2] = (z);                                                          \
    (out)[3] = (w);                                                          \
  } while (0)

#define OWL_IDENTITY_M4(m)                                                   \
  do {                                                                       \
    (m)[0][0] = 1.0F;                                                        \
    (m)[0][1] = 0.0F;                                                        \
    (m)[0][2] = 0.0F;                                                        \
    (m)[0][3] = 0.0F;                                                        \
    (m)[1][0] = 0.0F;                                                        \
    (m)[1][1] = 1.0F;                                                        \
    (m)[1][2] = 0.0F;                                                        \
    (m)[1][3] = 0.0F;                                                        \
    (m)[2][0] = 0.0F;                                                        \
    (m)[2][1] = 0.0F;                                                        \
    (m)[2][2] = 1.0F;                                                        \
    (m)[2][3] = 0.0F;                                                        \
    (m)[3][0] = 0.0F;                                                        \
    (m)[3][1] = 0.0F;                                                        \
    (m)[3][2] = 0.0F;                                                        \
    (m)[3][3] = 1.0F;                                                        \
  } while (0)

#define OWL_COPY_V2(in, out)                                                 \
  do {                                                                       \
    (out)[0] = (in)[0];                                                      \
    (out)[1] = (in)[1];                                                      \
  } while (0)

#define OWL_COPY_V3(in, out)                                                 \
  do {                                                                       \
    (out)[0] = (in)[0];                                                      \
    (out)[1] = (in)[1];                                                      \
    (out)[2] = (in)[2];                                                      \
  } while (0)

#define OWL_COPY_V4(in, out)                                                 \
  do {                                                                       \
    (out)[0] = (in)[0];                                                      \
    (out)[1] = (in)[1];                                                      \
    (out)[2] = (in)[2];                                                      \
    (out)[3] = (in)[3];                                                      \
  } while (0)

#define OWL_COPY_M4(in, out)                                                 \
  do {                                                                       \
    (out)[0][0] = (in)[0][0];                                                \
    (out)[0][1] = (in)[0][1];                                                \
    (out)[0][2] = (in)[0][2];                                                \
    (out)[0][3] = (in)[0][3];                                                \
    (out)[1][0] = (in)[1][0];                                                \
    (out)[1][1] = (in)[1][1];                                                \
    (out)[1][2] = (in)[1][2];                                                \
    (out)[1][3] = (in)[1][3];                                                \
    (out)[2][0] = (in)[2][0];                                                \
    (out)[2][1] = (in)[2][1];                                                \
    (out)[2][2] = (in)[2][2];                                                \
    (out)[2][3] = (in)[2][3];                                                \
    (out)[3][0] = (in)[3][0];                                                \
    (out)[3][1] = (in)[3][1];                                                \
    (out)[3][2] = (in)[3][2];                                                \
    (out)[3][3] = (in)[3][3];                                                \
  } while (0)

#define OWL_COPY_M3(in, out)                                                 \
  do {                                                                       \
    (out)[0][0] = (in)[0][0];                                                \
    (out)[0][1] = (in)[0][1];                                                \
    (out)[0][2] = (in)[0][2];                                                \
    (out)[1][0] = (in)[1][0];                                                \
    (out)[1][1] = (in)[1][1];                                                \
    (out)[1][2] = (in)[1][2];                                                \
    (out)[2][0] = (in)[2][0];                                                \
    (out)[2][1] = (in)[2][1];                                                \
    (out)[2][2] = (in)[2][2];                                                \
  } while (0)

#define OWL_DOT_V2(lhs, rhs) ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1])
#define OWL_DOT_V3(lhs, rhs)                                                 \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2])

#define OWL_SCALE_V3(v, scale, out)                                          \
  do {                                                                       \
    (out)[0] = (scale) * (v)[0];                                             \
    (out)[1] = (scale) * (v)[1];                                             \
    (out)[2] = (scale) * (v)[2];                                             \
  } while (0)

#define OWL_SCALE_V4(v, scale, out)                                          \
  do {                                                                       \
    (out)[0] = (scale) * (v)[0];                                             \
    (out)[1] = (scale) * (v)[1];                                             \
    (out)[2] = (scale) * (v)[2];                                             \
    (out)[3] = (scale) * (v)[3];                                             \
  } while (0)

#define OWL_INV_SCALE_V2(v, scale, out)                                      \
  do {                                                                       \
    (out)[0] = (v)[0] / (scale);                                             \
    (out)[1] = (v)[1] / (scale);                                             \
  } while (0)

#define OWL_INV_SCALE_V3(v, scale, out)                                      \
  do {                                                                       \
    (out)[0] = (v)[0] / (scale);                                             \
    (out)[1] = (v)[1] / (scale);                                             \
    (out)[2] = (v)[2] / (scale);                                             \
  } while (0)

#define OWL_INV_SCALE_V4(v, scale, out)                                      \
  do {                                                                       \
    (out)[0] = (v)[0] / (scale);                                             \
    (out)[1] = (v)[1] / (scale);                                             \
    (out)[2] = (v)[2] / (scale);                                             \
    (out)[3] = (v)[3] / (scale);                                             \
  } while (0)

#define OWL_NEGATE_V3(v, out)                                                \
  do {                                                                       \
    (out)[0] = -(v)[0];                                                      \
    (out)[1] = -(v)[1];                                                      \
    (out)[2] = -(v)[2];                                                      \
  } while (0)

#define OWL_NEGATE_V4(v, out)                                                \
  do {                                                                       \
    (out)[0] = -(v)[0];                                                      \
    (out)[1] = -(v)[1];                                                      \
    (out)[2] = -(v)[2];                                                      \
    (out)[3] = -(v)[4];                                                      \
  } while (0)

#define OWL_ADD_V2(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] + (rhs)[0];                                          \
    (out)[1] = (lhs)[1] + (rhs)[1];                                          \
  } while (0)

#define OWL_ADD_V3(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] + (rhs)[0];                                          \
    (out)[1] = (lhs)[1] + (rhs)[1];                                          \
    (out)[2] = (lhs)[2] + (rhs)[2];                                          \
  } while (0)

#define OWL_ADD_V4(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] + (rhs)[0];                                          \
    (out)[1] = (lhs)[1] + (rhs)[1];                                          \
    (out)[2] = (lhs)[2] + (rhs)[2];                                          \
    (out)[3] = (lhs)[3] + (rhs)[3];                                          \
  } while (0)

#define OWL_SUB_V2(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] - (rhs)[0];                                          \
    (out)[1] = (lhs)[1] - (rhs)[1];                                          \
  } while (0)

#define OWL_SUB_V3(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] - (rhs)[0];                                          \
    (out)[1] = (lhs)[1] - (rhs)[1];                                          \
    (out)[2] = (lhs)[2] - (rhs)[2];                                          \
  } while (0)

#define OWL_SUB_V4(lhs, rhs, out)                                            \
  do {                                                                       \
    (out)[0] = (lhs)[0] - (rhs)[0];                                          \
    (out)[1] = (lhs)[1] - (rhs)[1];                                          \
    (out)[2] = (lhs)[2] - (rhs)[2];                                          \
    (out)[3] = (lhs)[3] - (rhs)[3];                                          \
  } while (0)

void owl_cross_v3(OwlV3 const lhs, OwlV3 const rhs, OwlV3 out);

void owl_mul_m4_v4(OwlM4 const m, OwlV4 const v, OwlV4 out);

float owl_mag_v2(OwlV2 const v);

float owl_mag_v3(OwlV3 const v);

void owl_norm_v3(OwlV3 const v, OwlV3 out);

void owl_make_rotate_m4(float angle, OwlV3 const axis, OwlM4 out);

void owl_translate_m4(OwlV3 const v, OwlM4 out);

void owl_ortho_m4(float left, float right, float bottom, float top,
                  float near, float far, OwlM4 out);

void owl_perspective_m4(float fov, float ratio, float near, float far,
                        OwlM4 out);

void owl_look_m4(OwlV3 const eye, OwlV3 const direction, OwlV3 const up,
                 OwlM4 out);

void owl_look_at_m4(OwlV3 const eye, OwlV3 const center, OwlV3 const up,
                    OwlM4 out);

void owl_find_dir_v3(float pitch, float yaw, OwlV3 const up, OwlV3 out);

void owl_mul_rot_m4(OwlM4 const lhs, OwlM4 const rhs, OwlM4 out);

void owl_rotate_m4(OwlM4 const m, float angle, OwlV3 const axis, OwlM4 out);

float owl_dist_v2(OwlV2 const from, OwlV2 const to);

float owl_dist_v3(OwlV3 const from, OwlV3 const to);

#ifndef NDEBUG
void owl_print_v2(OwlV2 const v);
void owl_print_v3(OwlV3 const v);
void owl_print_v4(OwlV4 const v);
void owl_print_m4(OwlM4 const m);
#endif

/* complex numbers */

void owl_mul_complex(OwlV2 const lhs, OwlV2 const rhs, OwlV2 out);

#endif

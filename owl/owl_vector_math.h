#ifndef OWL_VECTOR_MATH_H_
#define OWL_VECTOR_MATH_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

#define owl_deg2rad(a) ((a)*0.01745329252F)

#define owl_rad2deg(a) ((a)*57.2957795131F)

#define owl_v2_set(v, v1, v2)                                                 \
  do {                                                                        \
    (v)[0] = (v1);                                                            \
    (v)[1] = (v2);                                                            \
  } while (0)

#define owl_v3_set(v, v1, v2, v3)                                             \
  do {                                                                        \
    (v)[0] = (v1);                                                            \
    (v)[1] = (v2);                                                            \
    (v)[2] = (v3);                                                            \
  } while (0)

#define owl_v4_set(v, v1, v2, v3, v4)                                         \
  do {                                                                        \
    (v)[0] = (v1);                                                            \
    (v)[1] = (v2);                                                            \
    (v)[2] = (v3);                                                            \
    (v)[2] = (v4);                                                            \
  } while (0)

#define owl_v2_zero(v)                                                        \
  do {                                                                        \
    (v)[0] = 0.0F;                                                            \
    (v)[1] = 0.0F;                                                            \
  } while (0)

#define owl_v3_zero(v)                                                        \
  do {                                                                        \
    (v)[0] = 0.0F;                                                            \
    (v)[1] = 0.0F;                                                            \
    (v)[2] = 0.0F;                                                            \
  } while (0)

#define owl_v4_zero(v)                                                        \
  do {                                                                        \
    (v)[0] = 0.0F;                                                            \
    (v)[1] = 0.0F;                                                            \
    (v)[2] = 0.0F;                                                            \
    (v)[3] = 0.0F;                                                            \
  } while (0)

#define owl_m4_identity(m)                                                    \
  do {                                                                        \
    (m)[0][0] = 1.0F;                                                         \
    (m)[0][1] = 0.0F;                                                         \
    (m)[0][2] = 0.0F;                                                         \
    (m)[0][3] = 0.0F;                                                         \
    (m)[1][0] = 0.0F;                                                         \
    (m)[1][1] = 1.0F;                                                         \
    (m)[1][2] = 0.0F;                                                         \
    (m)[1][3] = 0.0F;                                                         \
    (m)[2][0] = 0.0F;                                                         \
    (m)[2][1] = 0.0F;                                                         \
    (m)[2][2] = 1.0F;                                                         \
    (m)[2][3] = 0.0F;                                                         \
    (m)[3][0] = 0.0F;                                                         \
    (m)[3][1] = 0.0F;                                                         \
    (m)[3][2] = 0.0F;                                                         \
    (m)[3][3] = 1.0F;                                                         \
  } while (0)

#define owl_v2_copy(in, out)                                                  \
  do {                                                                        \
    (out)[0] = (in)[0];                                                       \
    (out)[1] = (in)[1];                                                       \
  } while (0)

#define owl_v3_copy(in, out)                                                  \
  do {                                                                        \
    (out)[0] = (in)[0];                                                       \
    (out)[1] = (in)[1];                                                       \
    (out)[2] = (in)[2];                                                       \
  } while (0)

#define owl_v4_copy(in, out)                                                  \
  do {                                                                        \
    (out)[0] = (in)[0];                                                       \
    (out)[1] = (in)[1];                                                       \
    (out)[2] = (in)[2];                                                       \
    (out)[3] = (in)[3];                                                       \
  } while (0)

#define owl_m4_copy(in, out)                                                  \
  do {                                                                        \
    (out)[0][0] = (in)[0][0];                                                 \
    (out)[0][1] = (in)[0][1];                                                 \
    (out)[0][2] = (in)[0][2];                                                 \
    (out)[0][3] = (in)[0][3];                                                 \
    (out)[1][0] = (in)[1][0];                                                 \
    (out)[1][1] = (in)[1][1];                                                 \
    (out)[1][2] = (in)[1][2];                                                 \
    (out)[1][3] = (in)[1][3];                                                 \
    (out)[2][0] = (in)[2][0];                                                 \
    (out)[2][1] = (in)[2][1];                                                 \
    (out)[2][2] = (in)[2][2];                                                 \
    (out)[2][3] = (in)[2][3];                                                 \
    (out)[3][0] = (in)[3][0];                                                 \
    (out)[3][1] = (in)[3][1];                                                 \
    (out)[3][2] = (in)[3][2];                                                 \
    (out)[3][3] = (in)[3][3];                                                 \
  } while (0)

#define owl_m4_copy_v16(in, out)                                              \
  do {                                                                        \
    (out)[0][0] = (in)[0];                                                    \
    (out)[0][1] = (in)[1];                                                    \
    (out)[0][2] = (in)[2];                                                    \
    (out)[0][3] = (in)[3];                                                    \
    (out)[1][0] = (in)[4];                                                    \
    (out)[1][1] = (in)[5];                                                    \
    (out)[1][2] = (in)[6];                                                    \
    (out)[1][3] = (in)[7];                                                    \
    (out)[2][0] = (in)[8];                                                    \
    (out)[2][1] = (in)[9];                                                    \
    (out)[2][2] = (in)[10];                                                   \
    (out)[2][3] = (in)[11];                                                   \
    (out)[3][0] = (in)[12];                                                   \
    (out)[3][1] = (in)[13];                                                   \
    (out)[3][2] = (in)[14];                                                   \
    (out)[3][3] = (in)[15];                                                   \
  } while (0)

#define owl_m3_copy(in, out)                                                  \
  do {                                                                        \
    (out)[0][0] = (in)[0][0];                                                 \
    (out)[0][1] = (in)[0][1];                                                 \
    (out)[0][2] = (in)[0][2];                                                 \
    (out)[1][0] = (in)[1][0];                                                 \
    (out)[1][1] = (in)[1][1];                                                 \
    (out)[1][2] = (in)[1][2];                                                 \
    (out)[2][0] = (in)[2][0];                                                 \
    (out)[2][1] = (in)[2][1];                                                 \
    (out)[2][2] = (in)[2][2];                                                 \
  } while (0)

#define owl_v2_dot(lhs, rhs) ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1])
#define owl_v3_dot(lhs, rhs)                                                  \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2])

#define owl_v4_dot(lhs, rhs)                                                  \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2] +          \
   (lhs)[3] * (rhs)[3])

#define owl_v3_scale(v, scale, out)                                           \
  do {                                                                        \
    (out)[0] = (scale) * (v)[0];                                              \
    (out)[1] = (scale) * (v)[1];                                              \
    (out)[2] = (scale) * (v)[2];                                              \
  } while (0)

#define owl_v4_scale(v, scale, out)                                           \
  do {                                                                        \
    (out)[0] = (scale) * (v)[0];                                              \
    (out)[1] = (scale) * (v)[1];                                              \
    (out)[2] = (scale) * (v)[2];                                              \
    (out)[3] = (scale) * (v)[3];                                              \
  } while (0)

#define owl_m4_scale(m, scale, out)                                           \
  do {                                                                        \
    (out)[0][0] = (scale) * (m)[0][0];                                        \
    (out)[0][1] = (scale) * (m)[0][1];                                        \
    (out)[0][2] = (scale) * (m)[0][2];                                        \
    (out)[0][3] = (scale) * (m)[0][3];                                        \
    (out)[1][0] = (scale) * (m)[1][0];                                        \
    (out)[1][1] = (scale) * (m)[1][1];                                        \
    (out)[1][2] = (scale) * (m)[1][2];                                        \
    (out)[1][3] = (scale) * (m)[1][3];                                        \
    (out)[2][0] = (scale) * (m)[2][0];                                        \
    (out)[2][1] = (scale) * (m)[2][1];                                        \
    (out)[2][2] = (scale) * (m)[2][2];                                        \
    (out)[2][3] = (scale) * (m)[2][3];                                        \
    (out)[3][0] = (scale) * (m)[3][0];                                        \
    (out)[3][1] = (scale) * (m)[3][1];                                        \
    (out)[3][2] = (scale) * (m)[3][2];                                        \
    (out)[3][3] = (scale) * (m)[3][3];                                        \
  } while (0)

#define owl_v2_inv_scale(v, scale, out)                                       \
  do {                                                                        \
    (out)[0] = (v)[0] / (scale);                                              \
    (out)[1] = (v)[1] / (scale);                                              \
  } while (0)

#define owl_v3_inv_scale(v, scale, out)                                       \
  do {                                                                        \
    (out)[0] = (v)[0] / (scale);                                              \
    (out)[1] = (v)[1] / (scale);                                              \
    (out)[2] = (v)[2] / (scale);                                              \
  } while (0)

#define owl_v4_inv_scale(v, scale, out)                                       \
  do {                                                                        \
    (out)[0] = (v)[0] / (scale);                                              \
    (out)[1] = (v)[1] / (scale);                                              \
    (out)[2] = (v)[2] / (scale);                                              \
    (out)[3] = (v)[3] / (scale);                                              \
  } while (0)

#define owl_v3_negate(v, out)                                                 \
  do {                                                                        \
    (out)[0] = -(v)[0];                                                       \
    (out)[1] = -(v)[1];                                                       \
    (out)[2] = -(v)[2];                                                       \
  } while (0)

#define owl_v4_negate(v, out)                                                 \
  do {                                                                        \
    (out)[0] = -(v)[0];                                                       \
    (out)[1] = -(v)[1];                                                       \
    (out)[2] = -(v)[2];                                                       \
    (out)[3] = -(v)[3];                                                       \
  } while (0)

#define owl_v2_add(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] + (rhs)[0];                                           \
    (out)[1] = (lhs)[1] + (rhs)[1];                                           \
  } while (0)

#define owl_v3_add(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] + (rhs)[0];                                           \
    (out)[1] = (lhs)[1] + (rhs)[1];                                           \
    (out)[2] = (lhs)[2] + (rhs)[2];                                           \
  } while (0)

#define owl_v4_add(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] + (rhs)[0];                                           \
    (out)[1] = (lhs)[1] + (rhs)[1];                                           \
    (out)[2] = (lhs)[2] + (rhs)[2];                                           \
    (out)[3] = (lhs)[3] + (rhs)[3];                                           \
  } while (0)

#define owl_v2_sub(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] - (rhs)[0];                                           \
    (out)[1] = (lhs)[1] - (rhs)[1];                                           \
  } while (0)

#define owl_v3_sub(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] - (rhs)[0];                                           \
    (out)[1] = (lhs)[1] - (rhs)[1];                                           \
    (out)[2] = (lhs)[2] - (rhs)[2];                                           \
  } while (0)

#define owl_v4_sub(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] - (rhs)[0];                                           \
    (out)[1] = (lhs)[1] - (rhs)[1];                                           \
    (out)[2] = (lhs)[2] - (rhs)[2];                                           \
    (out)[3] = (lhs)[3] - (rhs)[3];                                           \
  } while (0)

#define owl_v3_mul(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] * (rhs)[0];                                           \
    (out)[1] = (lhs)[1] * (rhs)[1];                                           \
    (out)[2] = (lhs)[2] * (rhs)[2];                                           \
  } while (0)

#define owl_v4_mul(lhs, rhs, out)                                             \
  do {                                                                        \
    (out)[0] = (lhs)[0] * (rhs)[0];                                           \
    (out)[1] = (lhs)[1] * (rhs)[1];                                           \
    (out)[2] = (lhs)[2] * (rhs)[2];                                           \
    (out)[3] = (lhs)[3] * (rhs)[3];                                           \
  } while (0)

owl_public void
owl_v3_cross (owl_v3 const lhs, owl_v3 const rhs, owl_v3 out);

owl_public void
owl_m4v4_mul (owl_m4 const m, owl_v4 const v, owl_v4 out);

owl_public float
owl_v4_magnitude (owl_v2 const v);

owl_public float
owl_v3_magnitude (owl_v3 const v);

owl_public float
owl_v4_magnitude (owl_v3 const v);

owl_public void
owl_v3_normalize (owl_v3 const v, owl_v3 out);

owl_public void
owl_v4_normalize (owl_v4 const v, owl_v4 out);

owl_public void
owl_m4_make_rotate (float angle, owl_v3 const axis, owl_m4 out);

owl_public void
owl_m4_translate (owl_v3 const v, owl_m4 out);

owl_public void
owl_m4_ortho (float left, float right, float bot, float top, float near,
              float far, owl_m4 out);

owl_public void
owl_m4_perspective (float fov, float ratio, float near, float far, owl_m4 out);

owl_public void
owl_m4_look (owl_v3 const eye, owl_v3 const direction, owl_v3 const up,
             owl_m4 out);

owl_public void
owl_m4_look_at (owl_v3 const eye, owl_v3 const center, owl_v3 const up,
                owl_m4 out);

owl_public void
owl_v3_direction (float pitch, float yaw, owl_v3 const up, owl_v3 out);

owl_public void
owl_m4_multiply (owl_m4 const lhs, owl_m4 const rhs, owl_m4 out);

owl_public void
owl_m4_rotate (owl_m4 const m, float angle, owl_v3 const axis, owl_m4 out);

owl_public float
owl_v2_distance (owl_v2 const src, owl_v2 const dst);

owl_public float
owl_v3_distance (owl_v3 const src, owl_v3 const dst);

owl_public void
owl_v3_mix (owl_v3 const src, owl_v3 const dst, float weight, owl_v3 out);

owl_public void
owl_v4_mix (owl_v4 const src, owl_v4 const dst, float weight, owl_v4 out);

owl_public void
owl_q4_as_m4 (owl_q4 const q, owl_m4 out);

owl_public void
owl_m4_scale_v3 (owl_m4 const src, owl_v3 const scale, owl_m4 out);

owl_public void
owl_v4_quat_slerp (owl_v4 const src, owl_v4 const dst, float t, owl_v4 out);

owl_public void
owl_m4_inverse (owl_m4 const mat, owl_m4 dst);

#ifndef NDEBUG

#define OWL_V2_FORMAT         "%.8fF,%.8fF"
#define OWL_V2_FORMAT_ARGS(v) v[0], v[1]

#define OWL_V3_FORMAT         "%.8fF,%.8fF,%.8fF"
#define OWL_V3_FORMAT_ARGS(v) v[0], v[1], v[2]

#define OWL_V4_FORMAT         "%.8fF,%.8fF,%.8fF,%.8fF"
#define OWL_V4_FORMAT_ARGS(v) v[0], v[1], v[2], v[3]

#define OWL_M4_FORMAT                                                         \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                \
  "%.8fF,%.8fF,%.8fF,%.8fF"

/* clang-format off */

#define OWL_M4_FORMAT_ARGS(v)                                                  \
  v[0][0], v[0][1], v[0][2], v[0][3],                                          \
  v[1][0], v[1][1], v[1][2], v[1][3],                                          \
  v[2][0], v[2][1], v[2][2], v[2][3],                                          \
  v[3][0], v[3][1], v[3][2], v[3][3]

/* clang-format on */

owl_public void
owl_v2_print (owl_v2 const v);

owl_public void
owl_v3_print (owl_v3 const v);

owl_public void
owl_v4_print (owl_v4 const v);

owl_public void
owl_m4_print (owl_m4 const m);

#endif /* NDEBUG */

OWL_END_DECLS

#endif

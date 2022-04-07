#ifndef OWL_VECTOR_MATH_H_
#define OWL_VECTOR_MATH_H_

#include "owl_types.h"

/* math */
#define OWL_DEG_AS_RAD(a) ((a)*0.01745329252F)
#define OWL_RAD_AS_DEG(a) ((a)*57.2957795131F)

#define OWL_V2_ZERO(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
  } while (0)

#define OWL_V3_ZERO(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
    (v)[2] = 0.0F;                                                             \
  } while (0)

#define OWL_V4_ZERO(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
    (v)[2] = 0.0F;                                                             \
    (v)[3] = 0.0F;                                                             \
  } while (0)

#define OWL_V2_SET(x, y, out)                                                  \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
  } while (0)

#define OWL_V3_SET(x, y, z, out)                                               \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
    (out)[2] = (z);                                                            \
  } while (0)

#define OWL_V4_SET(x, y, z, w, out)                                            \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
    (out)[2] = (z);                                                            \
    (out)[3] = (w);                                                            \
  } while (0)

#define OWL_M4_IDENTITY(m)                                                     \
  do {                                                                         \
    (m)[0][0] = 1.0F;                                                          \
    (m)[0][1] = 0.0F;                                                          \
    (m)[0][2] = 0.0F;                                                          \
    (m)[0][3] = 0.0F;                                                          \
    (m)[1][0] = 0.0F;                                                          \
    (m)[1][1] = 1.0F;                                                          \
    (m)[1][2] = 0.0F;                                                          \
    (m)[1][3] = 0.0F;                                                          \
    (m)[2][0] = 0.0F;                                                          \
    (m)[2][1] = 0.0F;                                                          \
    (m)[2][2] = 1.0F;                                                          \
    (m)[2][3] = 0.0F;                                                          \
    (m)[3][0] = 0.0F;                                                          \
    (m)[3][1] = 0.0F;                                                          \
    (m)[3][2] = 0.0F;                                                          \
    (m)[3][3] = 1.0F;                                                          \
  } while (0)

#define OWL_V2_COPY(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
  } while (0)

#define OWL_V3_COPY(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
    (out)[2] = (in)[2];                                                        \
  } while (0)

#define OWL_V4_COPY(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
    (out)[2] = (in)[2];                                                        \
    (out)[3] = (in)[3];                                                        \
  } while (0)

#define OWL_M4_COPY(in, out)                                                   \
  do {                                                                         \
    (out)[0][0] = (in)[0][0];                                                  \
    (out)[0][1] = (in)[0][1];                                                  \
    (out)[0][2] = (in)[0][2];                                                  \
    (out)[0][3] = (in)[0][3];                                                  \
    (out)[1][0] = (in)[1][0];                                                  \
    (out)[1][1] = (in)[1][1];                                                  \
    (out)[1][2] = (in)[1][2];                                                  \
    (out)[1][3] = (in)[1][3];                                                  \
    (out)[2][0] = (in)[2][0];                                                  \
    (out)[2][1] = (in)[2][1];                                                  \
    (out)[2][2] = (in)[2][2];                                                  \
    (out)[2][3] = (in)[2][3];                                                  \
    (out)[3][0] = (in)[3][0];                                                  \
    (out)[3][1] = (in)[3][1];                                                  \
    (out)[3][2] = (in)[3][2];                                                  \
    (out)[3][3] = (in)[3][3];                                                  \
  } while (0)

#define OWL_M4_COPY_V16(in, out)                                               \
  do {                                                                         \
    (out)[0][0] = (in)[0];                                                     \
    (out)[0][1] = (in)[1];                                                     \
    (out)[0][2] = (in)[2];                                                     \
    (out)[0][3] = (in)[3];                                                     \
    (out)[1][0] = (in)[4];                                                     \
    (out)[1][1] = (in)[5];                                                     \
    (out)[1][2] = (in)[6];                                                     \
    (out)[1][3] = (in)[7];                                                     \
    (out)[2][0] = (in)[8];                                                     \
    (out)[2][1] = (in)[9];                                                     \
    (out)[2][2] = (in)[10];                                                    \
    (out)[2][3] = (in)[11];                                                    \
    (out)[3][0] = (in)[12];                                                    \
    (out)[3][1] = (in)[13];                                                    \
    (out)[3][2] = (in)[14];                                                    \
    (out)[3][3] = (in)[15];                                                    \
  } while (0)

#define OWL_M3_COPY(in, out)                                                   \
  do {                                                                         \
    (out)[0][0] = (in)[0][0];                                                  \
    (out)[0][1] = (in)[0][1];                                                  \
    (out)[0][2] = (in)[0][2];                                                  \
    (out)[1][0] = (in)[1][0];                                                  \
    (out)[1][1] = (in)[1][1];                                                  \
    (out)[1][2] = (in)[1][2];                                                  \
    (out)[2][0] = (in)[2][0];                                                  \
    (out)[2][1] = (in)[2][1];                                                  \
    (out)[2][2] = (in)[2][2];                                                  \
  } while (0)

#define OWL_V2_DOT(lhs, rhs) ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1])
#define OWL_V3_DOT(lhs, rhs)                                                   \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2])

#define OWL_V4_DOT(lhs, rhs)                                                   \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2] +           \
   (lhs)[3] * (rhs)[3])

#define OWL_V3_SCALE(v, scale, out)                                            \
  do {                                                                         \
    (out)[0] = (scale) * (v)[0];                                               \
    (out)[1] = (scale) * (v)[1];                                               \
    (out)[2] = (scale) * (v)[2];                                               \
  } while (0)

#define OWL_V4_SCALE(v, scale, out)                                            \
  do {                                                                         \
    (out)[0] = (scale) * (v)[0];                                               \
    (out)[1] = (scale) * (v)[1];                                               \
    (out)[2] = (scale) * (v)[2];                                               \
    (out)[3] = (scale) * (v)[3];                                               \
  } while (0)

#define OWL_M4_SCALE(m, scale, out)                                            \
  do {                                                                         \
    (out)[0][0] = (scale) * (m)[0][0];                                         \
    (out)[0][1] = (scale) * (m)[0][1];                                         \
    (out)[0][2] = (scale) * (m)[0][2];                                         \
    (out)[0][3] = (scale) * (m)[0][3];                                         \
    (out)[1][0] = (scale) * (m)[1][0];                                         \
    (out)[1][1] = (scale) * (m)[1][1];                                         \
    (out)[1][2] = (scale) * (m)[1][2];                                         \
    (out)[1][3] = (scale) * (m)[1][3];                                         \
    (out)[2][0] = (scale) * (m)[2][0];                                         \
    (out)[2][1] = (scale) * (m)[2][1];                                         \
    (out)[2][2] = (scale) * (m)[2][2];                                         \
    (out)[2][3] = (scale) * (m)[2][3];                                         \
    (out)[3][0] = (scale) * (m)[3][0];                                         \
    (out)[3][1] = (scale) * (m)[3][1];                                         \
    (out)[3][2] = (scale) * (m)[3][2];                                         \
    (out)[3][3] = (scale) * (m)[3][3];                                         \
  } while (0)

#define OWL_V2_INVERSE_SCALE(v, scale, out)                                    \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
  } while (0)

#define OWL_V3_INVERSE_SCALE(v, scale, out)                                    \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
    (out)[2] = (v)[2] / (scale);                                               \
  } while (0)

#define OWL_V4_INVERSE_SCALE(v, scale, out)                                    \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
    (out)[2] = (v)[2] / (scale);                                               \
    (out)[3] = (v)[3] / (scale);                                               \
  } while (0)

#define OWL_V3_NEGATE(v, out)                                                  \
  do {                                                                         \
    (out)[0] = -(v)[0];                                                        \
    (out)[1] = -(v)[1];                                                        \
    (out)[2] = -(v)[2];                                                        \
  } while (0)

#define OWL_V4_NEGATE(v, out)                                                  \
  do {                                                                         \
    (out)[0] = -(v)[0];                                                        \
    (out)[1] = -(v)[1];                                                        \
    (out)[2] = -(v)[2];                                                        \
    (out)[3] = -(v)[3];                                                        \
  } while (0)

#define OWL_V2_ADD(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
  } while (0)

#define OWL_V3_ADD(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
    (out)[2] = (lhs)[2] + (rhs)[2];                                            \
  } while (0)

#define OWL_V4_ADD(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
    (out)[2] = (lhs)[2] + (rhs)[2];                                            \
    (out)[3] = (lhs)[3] + (rhs)[3];                                            \
  } while (0)

#define OWL_V2_SUB(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
  } while (0)

#define OWL_V3_SUB(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
    (out)[2] = (lhs)[2] - (rhs)[2];                                            \
  } while (0)

#define OWL_V4_SUB(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
    (out)[2] = (lhs)[2] - (rhs)[2];                                            \
    (out)[3] = (lhs)[3] - (rhs)[3];                                            \
  } while (0)

#define OWL_V3_MUL(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] * (rhs)[0];                                            \
    (out)[1] = (lhs)[1] * (rhs)[1];                                            \
    (out)[2] = (lhs)[2] * (rhs)[2];                                            \
  } while (0)

#define OWL_V4_MUL(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] * (rhs)[0];                                            \
    (out)[1] = (lhs)[1] * (rhs)[1];                                            \
    (out)[2] = (lhs)[2] * (rhs)[2];                                            \
    (out)[3] = (lhs)[3] * (rhs)[3];                                            \
  } while (0)

void owl_v3_cross(owl_v3 const lhs, owl_v3 const rhs, owl_v3 out);

void owl_m4v4_mul(owl_m4 const m, owl_v4 const v, owl_v4 out);

float owl_v4_magnitude(owl_v2 const v);

float owl_v3_magnitude(owl_v3 const v);

float owl_v4_magnitude(owl_v3 const v);

void owl_v3_normalize(owl_v3 const v, owl_v3 out);

void owl_v4_normalize(owl_v4 const v, owl_v4 out);

void owl_m4_make_rotate(float angle, owl_v3 const axis, owl_m4 out);

void owl_m4_translate(owl_v3 const v, owl_m4 out);

void owl_m4_ortho(float left, float right, float bot, float top, float near,
                  float far, owl_m4 out);

void owl_m4_perspective(float fov, float ratio, float near, float far,
                        owl_m4 out);

void owl_m4_look(owl_v3 const eye, owl_v3 const direction, owl_v3 const up,
                 owl_m4 out);

void owl_m4_look_at(owl_v3 const eye, owl_v3 const center, owl_v3 const up,
                    owl_m4 out);

void owl_v3_direction(float pitch, float yaw, owl_v3 const up, owl_v3 out);

void owl_m4_multiply(owl_m4 const lhs, owl_m4 const rhs, owl_m4 out);

void owl_m4_rotate(owl_m4 const m, float angle, owl_v3 const axis, owl_m4 out);

float owl_v2_distance(owl_v2 const src, owl_v2 const dst);

float owl_v3_distance(owl_v3 const src, owl_v3 const dst);

void owl_v3_mix(owl_v3 const src, owl_v3 const dst, float weight, owl_v3 out);

void owl_v4_mix(owl_v4 const src, owl_v4 const dst, float weight, owl_v4 out);

void owl_q4_as_m4(owl_q4 const src, owl_m4 out);

void owl_m4_scale(owl_m4 const src, owl_v3 const scale, owl_m4 out);

void owl_v4_quat_slerp(owl_v4 const src, owl_v4 const dst, float t, owl_v4 out);

void owl_m4_inverse(owl_m4 const mat, owl_m4 dst);

#ifndef NDEBUG

#define OWL_V2_FORMAT "%.8fF,%.8fF"
#define OWL_V2_FORMAT_ARGS(v) v[0], v[1]

#define OWL_V3_FORMAT "%.8fF,%.8fF,%.8fF"
#define OWL_V3_FORMAT_ARGS(v) v[0], v[1], v[2]

#define OWL_V4_FORMAT "%.8fF,%.8fF,%.8fF,%.8fF"
#define OWL_V4_FORMAT_ARGS(v) v[0], v[1], v[2], v[3]

#define OWL_M4_FORMAT                                                          \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                 \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                 \
  "%.8fF,%.8fF,%.8fF,%.8fF,\n"                                                 \
  "%.8fF,%.8fF,%.8fF,%.8fF"

/* clang-format off */

#define OWL_M4_FORMAT_ARGS(v)                                                  \
  v[0][0], v[0][1], v[0][2], v[0][3],                                          \
  v[1][0], v[1][1], v[1][2], v[1][3],                                          \
  v[2][0], v[2][1], v[2][2], v[2][3],                                          \
  v[3][0], v[3][1], v[3][2], v[3][3]

/* clang-format on */

void owl_v2_print(owl_v2 const v);
void owl_v3_print(owl_v3 const v);
void owl_v4_print(owl_v4 const v);
void owl_m4_print(owl_m4 const m);

#endif /* NDEBUG */

#endif

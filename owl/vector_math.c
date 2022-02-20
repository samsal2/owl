#include "vector_math.h"

#include <math.h>

#ifndef OWL_SQRTF
#include <math.h>
#define OWL_SQRTF sqrtf
#define OWL_COSF cosf
#define OWL_SINF sinf
#endif

void owl_v3_cross(owl_v3 const lhs, owl_v3 const rhs, owl_v3 out) {
  owl_v3 lhs_tmp;
  owl_v3 rhs_tmp;

  OWL_V3_COPY(lhs, lhs_tmp);
  OWL_V3_COPY(rhs, rhs_tmp);

  out[0] = lhs_tmp[1] * rhs_tmp[2] - lhs_tmp[2] * rhs_tmp[1];
  out[1] = lhs_tmp[2] * rhs_tmp[0] - lhs_tmp[0] * rhs_tmp[2];
  out[2] = lhs_tmp[0] * rhs_tmp[1] - lhs_tmp[1] * rhs_tmp[0];
}

void owl_m4v4_mul(owl_m4 const m, owl_v4 const v, owl_v4 out) {
  owl_v4 tmp;
  OWL_V4_COPY(v, tmp);

  out[0] =
      m[0][0] * tmp[0] + m[1][0] * tmp[1] + m[2][0] * tmp[2] + m[3][0] * tmp[3];
  out[1] =
      m[0][1] * tmp[0] + m[1][1] * tmp[1] + m[2][1] * tmp[2] + m[3][1] * tmp[3];
  out[2] =
      m[0][2] * tmp[0] + m[1][2] * tmp[1] + m[2][2] * tmp[2] + m[3][2] * tmp[3];
  out[3] =
      m[0][3] * tmp[0] + m[1][3] * tmp[1] + m[2][3] * tmp[2] + m[3][3] * tmp[3];
}

float owl_v2_mag(owl_v2 const v) { return OWL_SQRTF(OWL_V2_DOT(v, v)); }

float owl_v3_mag(owl_v3 const v) { return OWL_SQRTF(OWL_V3_DOT(v, v)); }

void owl_v3_normalize(owl_v3 const v, owl_v3 out) {
  float const mag = 1 / owl_v3_mag(v);
  OWL_V3_SCALE(v, mag, out);
}

void owl_m4_make_rotate(float angle, owl_v3 const axis, owl_m4 out) {
  owl_v3 normalized_axis;
  owl_v3 v;
  owl_v3 vs;

  float const cos_angle = OWL_COSF(angle);
  float const sin_angle = OWL_SINF(angle);
  float const inv_cos_angle = 1.0F - cos_angle;

  owl_v3_normalize(axis, normalized_axis);
  OWL_V3_SCALE(normalized_axis, inv_cos_angle, v);
  OWL_V3_SCALE(normalized_axis, sin_angle, vs);
  OWL_V3_SCALE(normalized_axis, v[0], out[0]);
  OWL_V3_SCALE(normalized_axis, v[1], out[1]);
  OWL_V3_SCALE(normalized_axis, v[2], out[2]);

  out[0][0] += cos_angle;
  out[0][1] += vs[2];
  out[0][2] -= vs[1];
  out[0][3] = 0.0F;

  out[1][0] -= vs[2];
  out[1][1] += cos_angle;
  out[1][2] += vs[0];
  out[1][3] = 0.0F;

  out[2][0] += vs[1];
  out[2][1] -= vs[0];
  out[2][2] += cos_angle;
  out[2][3] = 0.0F;

  out[3][0] = 0.0F;
  out[3][1] = 0.0F;
  out[3][2] = 0.0F;
  out[3][3] = 1.0F;
}

void owl_m4_translate(owl_v3 const v, owl_m4 in_out) {
  owl_v4 v1;
  owl_v4 v2;
  owl_v4 v3;

  OWL_V4_SCALE(in_out[0], v[0], v1);
  OWL_V4_SCALE(in_out[1], v[1], v2);
  OWL_V4_SCALE(in_out[2], v[2], v3);

  OWL_V4_ADD(v1, in_out[3], in_out[3]);
  OWL_V4_ADD(v2, in_out[3], in_out[3]);
  OWL_V4_ADD(v3, in_out[3], in_out[3]);
}

void owl_m4_ortho(float left, float right, float bottom, float top, float near,
                  float far, owl_m4 out) {
  float const right_left = 2.0F / (right - left);
  float const tobottom = 2.0F / (top - bottom);
  float const far_near = 1.0F / (far - near);

  out[0][0] = 2.0F * right_left;
  out[0][1] = 0.0F;
  out[0][2] = 0.0F;
  out[0][3] = 0.0F;

  out[1][0] = 0.0F;
  out[1][1] = 2.0F * tobottom;
  out[1][2] = 0.0F;
  out[1][3] = 0.0F;

  out[2][0] = 0.0F;
  out[2][1] = 0.0F;
  out[2][2] = -1.0F * far_near;
  out[2][3] = 0.0F;

  out[3][0] = -(right + left) * right_left;
  out[3][1] = -(top + bottom) * tobottom;
  out[3][2] = -near * far_near;
  out[3][3] = 1.0F;
}

void owl_m4_perspective(float fov, float ratio, float near, float far,
                        owl_m4 out) {
  float const focal_length = 1.0F / tanf(fov * 0.5f);
  float const inv_diff_far_near = 1.0F / (far - near);

  out[0][0] = focal_length / ratio;
  out[0][1] = 0.0F;
  out[0][2] = 0.0F;
  out[0][3] = 0.0F;

  out[1][0] = 0.0F;
  out[1][1] = focal_length;
  out[1][2] = 0.0F;
  out[1][3] = 0.0F;

  out[2][0] = 0.0F;
  out[2][1] = 0.0F;
  out[2][2] = -far * inv_diff_far_near;
  out[2][3] = -1.0F;

  out[3][0] = 0.0F;
  out[3][1] = 0.0F;
  out[3][2] = -far * near * inv_diff_far_near;
  out[3][3] = 0.0F;
}

void owl_m4_look(owl_v3 const eye, owl_v3 const direction, owl_v3 const up,
                 owl_m4 out) {
  owl_v3 f;
  owl_v3 s;
  owl_v3 u;

  owl_v3_normalize(direction, f);
  owl_v3_cross(up, f, s);
  owl_v3_normalize(s, s);
  owl_v3_cross(f, s, u);

  out[0][0] = s[0];
  out[0][1] = u[0];
  out[0][2] = f[0];
  out[0][3] = 0.0F;

  out[1][0] = s[1];
  out[1][1] = u[1];
  out[1][2] = f[1];
  out[1][3] = 0.0F;

  out[2][0] = s[2];
  out[2][1] = u[2];
  out[2][2] = f[2];
  out[2][3] = 0.0F;

  out[3][0] = -OWL_V3_DOT(s, eye);
  out[3][1] = -OWL_V3_DOT(u, eye);
  out[3][2] = -OWL_V3_DOT(f, eye);
  out[3][3] = 1.0F;
}

void owl_m4_look_at(owl_v3 const eye, owl_v3 const center, owl_v3 const up,
                    owl_m4 out) {
  owl_v3 direction;
  OWL_V3_SUB(eye, center, direction);
  owl_m4_look(eye, direction, up, out);
}

void owl_v3_direction(float pitch, float yaw, owl_v3 const up, owl_v3 out) {
  owl_v4 direction;
  owl_v4 side;
  owl_m4 yaw_rotation;
  owl_m4 pitch_rotation;

  OWL_V4_SET(0.0F, 0.0F, -1.0F, 1.0F, direction); /* set direction */
  owl_v3_cross(direction, up, side);              /* find the side vector */

  owl_m4_make_rotate(pitch, side, pitch_rotation); /* find pitch rotation */
  owl_m4v4_mul(pitch_rotation, direction, direction);

  owl_m4_make_rotate(yaw, up, yaw_rotation); /* find yaw rotation */
  owl_m4v4_mul(yaw_rotation, direction, direction);

  OWL_V3_COPY(direction, out); /* set out */
}

void owl_m4_rotate(owl_m4 const m, float angle, owl_v3 const axis, owl_m4 out) {
  owl_m4 rotation;
  owl_m4_make_rotate(angle, axis, rotation);
  owl_m4_mul_rotation(m, rotation, out);
}

void owl_m4_mul_rotation(owl_m4 const lhs, owl_m4 const rhs, owl_m4 out) {
  owl_m4 a;
  owl_m3 b;
  owl_v4 c;

  OWL_M4_COPY(lhs, a);
  OWL_M3_COPY(rhs, b);
  OWL_V4_COPY(rhs[3], c);

  out[0][0] = a[0][0] * b[0][0] + a[1][0] * b[0][1] + a[2][0] * b[0][2];
  out[0][1] = a[0][1] * b[0][0] + a[1][1] * b[0][1] + a[2][1] * b[0][2];
  out[0][2] = a[0][2] * b[0][0] + a[1][2] * b[0][1] + a[2][2] * b[0][2];
  out[0][3] = a[0][3] * b[0][0] + a[1][3] * b[0][1] + a[2][3] * b[0][2];

  out[1][0] = a[0][0] * b[1][0] + a[1][0] * b[1][1] + a[2][0] * b[1][2];
  out[1][1] = a[0][1] * b[1][0] + a[1][1] * b[1][1] + a[2][1] * b[1][2];
  out[1][2] = a[0][2] * b[1][0] + a[1][2] * b[1][1] + a[2][2] * b[1][2];
  out[1][3] = a[0][3] * b[1][0] + a[1][3] * b[1][1] + a[2][3] * b[1][2];

  out[2][0] = a[0][0] * b[2][0] + a[1][0] * b[2][1] + a[2][0] * b[2][2];
  out[2][1] = a[0][1] * b[2][0] + a[1][1] * b[2][1] + a[2][1] * b[2][2];
  out[2][2] = a[0][2] * b[2][0] + a[1][2] * b[2][1] + a[2][2] * b[2][2];
  out[2][3] = a[0][3] * b[2][0] + a[1][3] * b[2][1] + a[2][3] * b[2][2];

  out[3][0] = a[0][0] * c[0] + a[1][0] * c[1] + a[2][0] * c[2] + a[3][0] * c[3];
  out[3][1] = a[0][1] * c[0] + a[1][1] * c[1] + a[2][1] * c[2] + a[3][1] * c[3];
  out[3][2] = a[0][2] * c[0] + a[1][2] * c[1] + a[2][2] * c[2] + a[3][2] * c[3];
  out[3][3] = a[0][3] * c[0] + a[1][3] * c[1] + a[2][3] * c[2] + a[3][3] * c[3];
}

float owl_v2_distance(owl_v2 const from, owl_v2 const to) {
  owl_v2 diff;
  OWL_V2_SUB(from, to, diff);
  return owl_v2_mag(diff);
}

float owl_v3_distance(owl_v3 const from, owl_v3 const to) {
  owl_v3 diff;
  OWL_V3_SUB(from, to, diff);
  return owl_v3_mag(diff);
}

#ifndef NDEBUG

#include <stdio.h>

void owl_v2_print(owl_v2 const v) {
  printf(OWL_V2_FORMAT, OWL_V2_FORMAT_ARGS(v));
}

void owl_v3_print(owl_v3 const v) {
  printf(OWL_V3_FORMAT, OWL_V3_FORMAT_ARGS(v));
}

void owl_v4_print(owl_v4 const v) {
  printf(OWL_V4_FORMAT, OWL_V4_FORMAT_ARGS(v));
}

void owl_m4_print(owl_m4 const m) {
  printf(OWL_M4_FORMAT, OWL_M4_FORMAT_ARGS(m));
}

#endif

void owl_complex_mul(owl_v2 const lhs, owl_v2 const rhs, owl_v2 out) {
  owl_v2 lhs_tmp;
  owl_v2 rhs_tmp;

  OWL_V2_COPY(lhs, lhs_tmp);
  OWL_V2_COPY(rhs, rhs_tmp);

  out[0] = (lhs_tmp[0] * rhs_tmp[0] - lhs_tmp[1] * rhs_tmp[1]);
  out[1] = (lhs_tmp[0] * rhs_tmp[1] + lhs_tmp[1] * rhs_tmp[0]);
}

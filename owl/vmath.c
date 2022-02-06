#include "vmath.h"

#include <math.h>

#ifndef OWL_SQRTF
#include <math.h>
#define OWL_SQRTF sqrtf
#define OWL_COSF cosf
#define OWL_SINF sinf
#endif

void owl_cross_v3(owl_v3 const lhs, owl_v3 const rhs, owl_v3 out) {
  owl_v3 lhs_tmp;
  owl_v3 rhs_tmp;

  OWL_COPY_V3(lhs, lhs_tmp);
  OWL_COPY_V3(rhs, rhs_tmp);

  out[0] = lhs_tmp[1] * rhs_tmp[2] - lhs_tmp[2] * rhs_tmp[1];
  out[1] = lhs_tmp[2] * rhs_tmp[0] - lhs_tmp[0] * rhs_tmp[2];
  out[2] = lhs_tmp[0] * rhs_tmp[1] - lhs_tmp[1] * rhs_tmp[0];
}

void owl_mul_m4_v4(owl_m4 const m, owl_v4 const v, owl_v4 out) {
  owl_v4 tmp;
  OWL_COPY_V4(v, tmp);

  out[0] =
      m[0][0] * tmp[0] + m[1][0] * tmp[1] + m[2][0] * tmp[2] + m[3][0] * tmp[3];
  out[1] =
      m[0][1] * tmp[0] + m[1][1] * tmp[1] + m[2][1] * tmp[2] + m[3][1] * tmp[3];
  out[2] =
      m[0][2] * tmp[0] + m[1][2] * tmp[1] + m[2][2] * tmp[2] + m[3][2] * tmp[3];
  out[3] =
      m[0][3] * tmp[0] + m[1][3] * tmp[1] + m[2][3] * tmp[2] + m[3][3] * tmp[3];
}

float owl_mag_v2(owl_v2 const v) { return OWL_SQRTF(OWL_DOT_V2(v, v)); }

float owl_mag_v3(owl_v3 const v) { return OWL_SQRTF(OWL_DOT_V3(v, v)); }

void owl_norm_v3(owl_v3 const v, owl_v3 out) {
  float const mag = 1 / owl_mag_v3(v);
  OWL_SCALE_V3(v, mag, out);
}

void owl_make_rotate_m4(float angle, owl_v3 const axis, owl_m4 out) {
  owl_v3 normalized_axis;
  owl_v3 v;
  owl_v3 vs;

  float const cos_angle = OWL_COSF(angle);
  float const sin_angle = OWL_SINF(angle);
  float const inv_cos_angle = 1.0F - cos_angle;

  owl_norm_v3(axis, normalized_axis);
  OWL_SCALE_V3(normalized_axis, inv_cos_angle, v);
  OWL_SCALE_V3(normalized_axis, sin_angle, vs);
  OWL_SCALE_V3(normalized_axis, v[0], out[0]);
  OWL_SCALE_V3(normalized_axis, v[1], out[1]);
  OWL_SCALE_V3(normalized_axis, v[2], out[2]);

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

void owl_translate_m4(owl_v3 const v, owl_m4 in_out) {
  owl_v4 v1;
  owl_v4 v2;
  owl_v4 v3;

  OWL_SCALE_V4(in_out[0], v[0], v1);
  OWL_SCALE_V4(in_out[1], v[1], v2);
  OWL_SCALE_V4(in_out[2], v[2], v3);

  OWL_ADD_V4(v1, in_out[3], in_out[3]);
  OWL_ADD_V4(v2, in_out[3], in_out[3]);
  OWL_ADD_V4(v3, in_out[3], in_out[3]);
}

void owl_ortho_m4(float left, float right, float bottom, float top, float near,
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

void owl_perspective_m4(float fov, float ratio, float near, float far,
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

void owl_look_m4(owl_v3 const eye, owl_v3 const direction, owl_v3 const up,
                 owl_m4 out) {
  owl_v3 f;
  owl_v3 s;
  owl_v3 u;

  owl_norm_v3(direction, f);
  owl_cross_v3(up, f, s);
  owl_norm_v3(s, s);
  owl_cross_v3(f, s, u);

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

  out[3][0] = -OWL_DOT_V3(s, eye);
  out[3][1] = -OWL_DOT_V3(u, eye);
  out[3][2] = -OWL_DOT_V3(f, eye);
  out[3][3] = 1.0F;
}

void owl_look_at_m4(owl_v3 const eye, owl_v3 const center, owl_v3 const up,
                    owl_m4 out) {
  owl_v3 direction;
  OWL_SUB_V3(eye, center, direction);
  owl_look_m4(eye, direction, up, out);
}

void owl_find_dir_v3(float pitch, float yaw, owl_v3 const up, owl_v3 out) {
  owl_v4 direction;
  owl_v4 side;
  owl_m4 yaw_rotation;
  owl_m4 pitch_rotation;

  OWL_SET_V4(0.0F, 0.0F, -1.0F, 1.0F, direction); /* set direction */
  owl_cross_v3(direction, up, side);              /* find the side vector */

  owl_make_rotate_m4(pitch, side, pitch_rotation); /* find pitch rotation */
  owl_mul_m4_v4(pitch_rotation, direction, direction);

  owl_make_rotate_m4(yaw, up, yaw_rotation); /* find yaw rotation */
  owl_mul_m4_v4(yaw_rotation, direction, direction);

  OWL_COPY_V3(direction, out); /* set out */
}

void owl_rotate_m4(owl_m4 const m, float angle, owl_v3 const axis, owl_m4 out) {
  owl_m4 rotation;
  owl_make_rotate_m4(angle, axis, rotation);
  owl_mul_rot_m4(m, rotation, out);
}

void owl_mul_rot_m4(owl_m4 const lhs, owl_m4 const rhs, owl_m4 out) {
  owl_m4 a;
  owl_m3 b;
  owl_v4 c;

  OWL_COPY_M4(lhs, a);
  OWL_COPY_M3(rhs, b);
  OWL_COPY_V4(rhs[3], c);

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

float owl_dist_v2(owl_v2 const from, owl_v2 const to) {
  owl_v2 diff;
  OWL_SUB_V2(from, to, diff);
  return owl_mag_v2(diff);
}

float owl_dist_v3(owl_v3 const from, owl_v3 const to) {
  owl_v3 diff;
  OWL_SUB_V3(from, to, diff);
  return owl_mag_v3(diff);
}

#ifndef NDEBUG

#include <stdio.h>

void owl_print_v2(owl_v2 const v) {
  printf(OWL_MATH_V2_FORMAT, OWL_MATH_V2_FORMAT_ARGS(v));
}

void owl_print_v3(owl_v3 const v) {
  printf(OWL_MATH_V3_FORMAT, OWL_MATH_V3_FORMAT_ARGS(v));
}

void owl_print_v4(owl_v4 const v) {
  printf(OWL_MATH_V4_FORMAT, OWL_MATH_V4_FORMAT_ARGS(v));
}

void owl_print_m4(owl_m4 const m) {
  printf(OWL_MATH_M4_FORMAT, OWL_MATH_M4_FORMAT_ARGS(m));
}

#endif

void owl_mul_complex(owl_v2 const lhs, owl_v2 const rhs, owl_v2 out) {
  owl_v2 lhs_tmp;
  owl_v2 rhs_tmp;

  OWL_COPY_V2(lhs, lhs_tmp);
  OWL_COPY_V2(rhs, rhs_tmp);

  out[0] = (lhs_tmp[0] * rhs_tmp[0] - lhs_tmp[1] * rhs_tmp[1]);
  out[1] = (lhs_tmp[0] * rhs_tmp[1] + lhs_tmp[1] * rhs_tmp[0]);
}

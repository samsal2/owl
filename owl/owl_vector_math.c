#include "owl_vector_math.h"

#include "owl_internal.h"

#include <math.h>

#ifndef OWL_SQRTF
#include <math.h>
#define OWL_SQRTF sqrtf
#define OWL_COSF cosf
#define OWL_SINF sinf
#define OWL_FABSF fabsf
#define OWL_ACOSF acosf
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

float owl_v2_magnitude(owl_v2 const v) { return OWL_SQRTF(OWL_V2_DOT(v, v)); }

float owl_v3_magnitude(owl_v3 const v) { return OWL_SQRTF(OWL_V3_DOT(v, v)); }

float owl_v4_magnitude(owl_v4 const v) { return OWL_SQRTF(OWL_V4_DOT(v, v)); }

void owl_v3_normalize(owl_v3 const v, owl_v3 out) {
  float const mag = 1 / owl_v3_magnitude(v);
  OWL_V3_SCALE(v, mag, out);
}

void owl_v4_normalize(owl_v4 const v, owl_v4 out) {
  float const mag = 1 / owl_v4_magnitude(v);
  OWL_V4_SCALE(v, mag, out);
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
  float const focal_length = 1.0F / tanf(fov * 0.5F);
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
  owl_m4_multiply(m, rotation, out);
}

void owl_m4_multiply(owl_m4 const lhs, owl_m4 const rhs, owl_m4 out) {
  float a00 = lhs[0][0];
  float a01 = lhs[0][1];
  float a02 = lhs[0][2];
  float a03 = lhs[0][3];
  float a10 = lhs[1][0];
  float a11 = lhs[1][1];
  float a12 = lhs[1][2];
  float a13 = lhs[1][3];
  float a20 = lhs[2][0];
  float a21 = lhs[2][1];
  float a22 = lhs[2][2];
  float a23 = lhs[2][3];
  float a30 = lhs[3][0];
  float a31 = lhs[3][1];
  float a32 = lhs[3][2];
  float a33 = lhs[3][3];
  float b00 = rhs[0][0];
  float b01 = rhs[0][1];
  float b02 = rhs[0][2];
  float b03 = rhs[0][3];
  float b10 = rhs[1][0];
  float b11 = rhs[1][1];
  float b12 = rhs[1][2];
  float b13 = rhs[1][3];
  float b20 = rhs[2][0];
  float b21 = rhs[2][1];
  float b22 = rhs[2][2];
  float b23 = rhs[2][3];
  float b30 = rhs[3][0];
  float b31 = rhs[3][1];
  float b32 = rhs[3][2];
  float b33 = rhs[3][3];

  out[0][0] = a00 * b00 + a10 * b01 + a20 * b02 + a30 * b03;
  out[0][1] = a01 * b00 + a11 * b01 + a21 * b02 + a31 * b03;
  out[0][2] = a02 * b00 + a12 * b01 + a22 * b02 + a32 * b03;
  out[0][3] = a03 * b00 + a13 * b01 + a23 * b02 + a33 * b03;
  out[1][0] = a00 * b10 + a10 * b11 + a20 * b12 + a30 * b13;
  out[1][1] = a01 * b10 + a11 * b11 + a21 * b12 + a31 * b13;
  out[1][2] = a02 * b10 + a12 * b11 + a22 * b12 + a32 * b13;
  out[1][3] = a03 * b10 + a13 * b11 + a23 * b12 + a33 * b13;
  out[2][0] = a00 * b20 + a10 * b21 + a20 * b22 + a30 * b23;
  out[2][1] = a01 * b20 + a11 * b21 + a21 * b22 + a31 * b23;
  out[2][2] = a02 * b20 + a12 * b21 + a22 * b22 + a32 * b23;
  out[2][3] = a03 * b20 + a13 * b21 + a23 * b22 + a33 * b23;
  out[3][0] = a00 * b30 + a10 * b31 + a20 * b32 + a30 * b33;
  out[3][1] = a01 * b30 + a11 * b31 + a21 * b32 + a31 * b33;
  out[3][2] = a02 * b30 + a12 * b31 + a22 * b32 + a32 * b33;
  out[3][3] = a03 * b30 + a13 * b31 + a23 * b32 + a33 * b33;
}

float owl_v2_distance(owl_v2 const src, owl_v2 const dst) {
  owl_v2 diff;
  OWL_V2_SUB(src, dst, diff);
  return owl_v2_magnitude(diff);
}

float owl_v3_distance(owl_v3 const src, owl_v3 const dst) {
  owl_v3 diff;
  OWL_V3_SUB(src, dst, diff);
  return owl_v3_magnitude(diff);
}

void owl_v3_mix(owl_v3 const src, owl_v3 const dst, float weight, owl_v3 out) {
  owl_v3 s;
  owl_v3 v;

  OWL_V3_SET(weight, weight, weight, s);
  OWL_V3_SUB(dst, src, v);
  OWL_V3_MUL(s, v, v);
  OWL_V3_ADD(src, v, out);
}

void owl_v4_mix(owl_v4 const src, owl_v4 const dst, float weight, owl_v4 out) {
  owl_v4 s;
  owl_v4 v;

  OWL_V4_SET(weight, weight, weight, weight, s);
  OWL_V4_SUB(dst, src, v);
  OWL_V4_MUL(s, v, v);
  OWL_V4_ADD(src, v, out);
}

void owl_q4_as_m4(owl_q4 const q, owl_m4 out) {
  float xx = q[0] * q[0];
  float yy = q[1] * q[1];
  float zz = q[2] * q[2];
  float xz = q[0] * q[2];
  float xy = q[0] * q[1];
  float yz = q[1] * q[2];
  float wx = q[3] * q[0];
  float wy = q[3] * q[1];
  float wz = q[3] * q[2];

  out[0][0] = 1.0F - 2.0F * (yy + zz);
  out[0][1] = 2.0F * (xy + wz);
  out[0][2] = 2.0F * (xz - wy);
  out[1][0] = 2.0F * (xy - wz);
  out[1][1] = 1.0F - 2.0F * (xx + zz);
  out[1][2] = 2.0F * (yz + wx);
  out[2][0] = 2.0F * (xz + wy);
  out[2][1] = 2.0F * (yz - wx);
  out[2][2] = 1.0F - 2.0F * (xx + yy);

  out[0][3] = 0.0F;
  out[1][3] = 0.0F;
  out[2][3] = 0.0F;
  out[3][0] = 0.0F;
  out[3][1] = 0.0F;
  out[3][2] = 0.0F;
  out[3][3] = 1.0F;
}

void owl_m4_scale(owl_m4 const src, owl_v3 const scale, owl_m4 out) {
  owl_m4 m;
  owl_v3 s;

  OWL_M4_COPY(src, m);
  OWL_V3_COPY(scale, s);

  OWL_V4_SCALE(m[0], s[0], out[0]);
  OWL_V4_SCALE(m[1], s[1], out[1]);
  OWL_V4_SCALE(m[2], s[2], out[2]);

  OWL_V4_COPY(m[3], out[3]);
}

OWL_INTERNAL void owl_v4_quat_lerp(owl_v4 const src, owl_v4 const dst, float t,
                                   owl_v4 dest) {
  owl_v4 s;
  owl_v4 v;

  /* src + s * (to - src) */
  OWL_V4_SET(t, t, t, t, s);
  OWL_V4_SUB(dst, src, v);
  OWL_V4_MUL(s, v, v);
  OWL_V4_ADD(src, v, dest);
}

void owl_v4_quat_slerp(owl_v4 const src, owl_v4 const dst, float t,
                       owl_v4 out) {
  owl_v4 q1;
  owl_v4 q2;
  float cosTheta;
  float sinTheta;
  float angle;

  cosTheta = OWL_V4_DOT(src, dst);
  OWL_V4_COPY(src, q1);

  if (OWL_FABSF(cosTheta) >= 1.0F) {
    OWL_V4_COPY(q1, out);
    return;
  }

  if (cosTheta < 0.0F) {
    OWL_V4_NEGATE(q1, q1);
    cosTheta = -cosTheta;
  }

  sinTheta = OWL_SQRTF(1.0F - cosTheta * cosTheta);

  /* LERP to avoid zero division */
  if (OWL_FABSF(sinTheta) < 0.001F) {
    owl_v4_quat_lerp(src, dst, t, out);
    return;
  }

  /* SLERP */
  angle = OWL_ACOSF(cosTheta);
  OWL_V4_SCALE(q1, OWL_SINF((1.0F - t) * angle), q1);
  OWL_V4_SCALE(dst, OWL_SINF(t * angle), q2);

  OWL_V4_ADD(q1, q2, q1);
  OWL_V4_SCALE(q1, 1.0F / sinTheta, out);
}

void owl_m4_inverse(owl_m4 const mat, owl_m4 out) {
  float t[6];
  float det;
  float a = mat[0][0];
  float b = mat[0][1];
  float c = mat[0][2];
  float d = mat[0][3];
  float e = mat[1][0];
  float f = mat[1][1];
  float g = mat[1][2];
  float h = mat[1][3];
  float i = mat[2][0];
  float j = mat[2][1];
  float k = mat[2][2];
  float l = mat[2][3];
  float m = mat[3][0];
  float n = mat[3][1];
  float o = mat[3][2];
  float p = mat[3][3];

  t[0] = k * p - o * l;
  t[1] = j * p - n * l;
  t[2] = j * o - n * k;
  t[3] = i * p - m * l;
  t[4] = i * o - m * k;
  t[5] = i * n - m * j;

  out[0][0] = f * t[0] - g * t[1] + h * t[2];
  out[1][0] = -(e * t[0] - g * t[3] + h * t[4]);
  out[2][0] = e * t[1] - f * t[3] + h * t[5];
  out[3][0] = -(e * t[2] - f * t[4] + g * t[5]);

  out[0][1] = -(b * t[0] - c * t[1] + d * t[2]);
  out[1][1] = a * t[0] - c * t[3] + d * t[4];
  out[2][1] = -(a * t[1] - b * t[3] + d * t[5]);
  out[3][1] = a * t[2] - b * t[4] + c * t[5];

  t[0] = g * p - o * h;
  t[1] = f * p - n * h;
  t[2] = f * o - n * g;
  t[3] = e * p - m * h;
  t[4] = e * o - m * g;
  t[5] = e * n - m * f;

  out[0][2] = b * t[0] - c * t[1] + d * t[2];
  out[1][2] = -(a * t[0] - c * t[3] + d * t[4]);
  out[2][2] = a * t[1] - b * t[3] + d * t[5];
  out[3][2] = -(a * t[2] - b * t[4] + c * t[5]);

  t[0] = g * l - k * h;
  t[1] = f * l - j * h;
  t[2] = f * k - j * g;
  t[3] = e * l - i * h;
  t[4] = e * k - i * g;
  t[5] = e * j - i * f;

  out[0][3] = -(b * t[0] - c * t[1] + d * t[2]);
  out[1][3] = a * t[0] - c * t[3] + d * t[4];
  out[2][3] = -(a * t[1] - b * t[3] + d * t[5]);
  out[3][3] = a * t[2] - b * t[4] + c * t[5];

  det = 1.0F / (a * out[0][0] + b * out[1][0] + c * out[2][0] + d * out[3][0]);

  OWL_M4_SCALE(out, det, out);
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

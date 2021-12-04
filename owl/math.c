#include <owl/math.h>

#ifndef OWL_SQRTF
#include <math.h>
#define OWL_SQRTF sqrtf
#define OWL_COSF cosf
#define OWL_SINF sinf
#endif

void owl_cross_v3(OwlV3 const lhs, OwlV3 const rhs, OwlV3 out) {
  OwlV3 lhs_tmp;
  OwlV3 rhs_tmp;

  OWL_COPY_V3(lhs, lhs_tmp);
  OWL_COPY_V3(rhs, rhs_tmp);

  out[0] = lhs_tmp[1] * rhs_tmp[2] - lhs_tmp[2] * rhs_tmp[1];
  out[1] = lhs_tmp[2] * rhs_tmp[0] - lhs_tmp[0] * rhs_tmp[2];
  out[2] = lhs_tmp[0] * rhs_tmp[1] - lhs_tmp[1] * rhs_tmp[0];
}

void owl_mul_m4_v4(OwlM4 const m, OwlV4 const v, OwlV4 out) {
  OwlV4 tmp;
  OWL_COPY_V4(v, tmp);

  out[0] = m[0][0] * tmp[0] + m[1][0] * tmp[1] + m[2][0] * tmp[2] +
           m[3][0] * tmp[3];
  out[1] = m[0][1] * tmp[0] + m[1][1] * tmp[1] + m[2][1] * tmp[2] +
           m[3][1] * tmp[3];
  out[2] = m[0][2] * tmp[0] + m[1][2] * tmp[1] + m[2][2] * tmp[2] +
           m[3][2] * tmp[3];
  out[3] = m[0][3] * tmp[0] + m[1][3] * tmp[1] + m[2][3] * tmp[2] +
           m[3][3] * tmp[3];
}

float owl_mag_v2(OwlV2 const v) { return OWL_SQRTF(OWL_DOT_V2(v, v)); }

float owl_mag_v3(OwlV3 const v) { return OWL_SQRTF(OWL_DOT_V3(v, v)); }

void owl_norm_v3(OwlV3 const v, OwlV3 out) {
  float const mag = 1 / owl_mag_v3(v);
  OWL_SCALE_V3(v, mag, out);
}

void owl_make_rotate_m4(float angle, OwlV3 const axis, OwlM4 out) {
  OwlV3 normalized_axis;
  OwlV3 v;
  OwlV3 vs;

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

void owl_translate_m4(OwlV3 const v, OwlM4 in_out) {
  OwlV4 v1;
  OwlV4 v2;
  OwlV4 v3;

  OWL_SCALE_V4(in_out[0], v[0], v1);
  OWL_SCALE_V4(in_out[1], v[1], v2);
  OWL_SCALE_V4(in_out[2], v[2], v3);

  OWL_ADD_V4(v1, in_out[3], in_out[3]);
  OWL_ADD_V4(v2, in_out[3], in_out[3]);
  OWL_ADD_V4(v3, in_out[3], in_out[3]);
}

void owl_ortho_m4(float left, float right, float bottom, float top,
                  float near, float far, OwlM4 out) {
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
                        OwlM4 out) {
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

void owl_look_m4(OwlV3 const eye, OwlV3 const direction, OwlV3 const up,
                 OwlM4 out) {
  OwlV3 f;
  OwlV3 s;
  OwlV3 u;

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

void owl_look_at_m4(OwlV3 const eye, OwlV3 const center, OwlV3 const up,
                    OwlM4 out) {
  OwlV3 direction;
  OWL_SUB_V3(eye, center, direction);
  owl_look_m4(eye, direction, up, out);
}

void owl_find_dir_v3(float pitch, float yaw, OwlV3 const up, OwlV3 out) {
  OwlV4 direction;
  OwlV4 side;
  OwlM4 yaw_rotation;
  OwlM4 pitch_rotation;

  OWL_SET_V4(0.0F, 0.0F, -1.0F, 1.0F, direction); /* set direction */
  owl_cross_v3(direction, up, side);              /* find the side vector */

  owl_make_rotate_m4(pitch, side, pitch_rotation); /* find pitch rotation */
  owl_mul_m4_v4(pitch_rotation, direction, direction);

  owl_make_rotate_m4(yaw, up, yaw_rotation); /* find yaw rotation */
  owl_mul_m4_v4(yaw_rotation, direction, direction);

  OWL_COPY_V3(direction, out); /* set out */
}

void owl_rotate_m4(OwlM4 const m, float angle, OwlV3 const axis, OwlM4 out) {
  OwlM4 rotation;
  owl_make_rotate_m4(angle, axis, rotation);
  owl_mul_rot_m4(m, rotation, out);
}

void owl_mul_rot_m4(OwlM4 const lhs, OwlM4 const rhs, OwlM4 out) {
  OwlM4 a;
  OtterM3 b;
  OwlV4 c;

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

  out[3][0] =
      a[0][0] * c[0] + a[1][0] * c[1] + a[2][0] * c[2] + a[3][0] * c[3];
  out[3][1] =
      a[0][1] * c[0] + a[1][1] * c[1] + a[2][1] * c[2] + a[3][1] * c[3];
  out[3][2] =
      a[0][2] * c[0] + a[1][2] * c[1] + a[2][2] * c[2] + a[3][2] * c[3];
  out[3][3] =
      a[0][3] * c[0] + a[1][3] * c[1] + a[2][3] * c[2] + a[3][3] * c[3];
}

float owl_dist_v2(OwlV2 const from, OwlV2 const to) {
  OwlV2 diff;
  OWL_SUB_V2(from, to, diff);
  return owl_mag_v2(diff);
}

float owl_dist_v3(OwlV3 const from, OwlV3 const to) {
  OwlV3 diff;
  OWL_SUB_V3(from, to, diff);
  return owl_mag_v3(diff);
}

#ifndef NDEBUG

#include <stdio.h>

void owl_print_v2(OwlV2 const v) { printf("%.4f %.4f", v[0], v[1]); }

void owl_print_v3(OwlV3 const v) {
  printf("%.4f %.4f %.4f", v[0], v[1], v[2]);
}

void owl_print_v4(OwlV4 const v) {
  printf("%.4f %.4f %.4f %.4f", v[0], v[1], v[2], v[3]);
}

void owl_print_m4(OwlM4 const m) {
  int i;
  for (i = 0; i < 4; ++i) {
    owl_print_v4(m[i]);
  }
}

#endif

void owl_mul_complex(OwlV2 const lhs, OwlV2 const rhs, OwlV2 out) {
  OwlV2 lhs_tmp;
  OwlV2 rhs_tmp;

  OWL_COPY_V2(lhs, lhs_tmp);
  OWL_COPY_V2(rhs, rhs_tmp);

  out[0] = (lhs_tmp[0] * rhs_tmp[0] - lhs_tmp[1] * rhs_tmp[1]);
  out[1] = (lhs_tmp[0] * rhs_tmp[1] + lhs_tmp[1] * rhs_tmp[0]);
}

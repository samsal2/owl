#ifndef OWL_INCLUDE_OWL_H_
#define OWL_INCLUDE_OWL_H_

/* clang-format on */

#include <stdint.h>
typedef uint8_t OwlU8;
typedef uint16_t OwlU16;
typedef uint32_t OwlU32;
typedef uint64_t OwlU64;
typedef unsigned char OwlByte;

typedef float OwlV2[2];
typedef float OwlV3[3];
typedef float OwlV4[4];

typedef int OwlV2I[2];
typedef int OwlV3I[3];
typedef int OwlV4I[4];

typedef OwlV2 OwlM2[2];
typedef OwlV3 OwlM3[3];
typedef OwlV4 OwlM4[4];

struct owl_vk_renderer;
struct owl_font;
struct owl_img;
struct owl_window;

enum owl_code {
  OWL_SUCCESS,
  OWL_ERROR_BAD_ALLOC,
  OWL_ERROR_BAD_INIT,
  OWL_ERROR_BAD_HANDLE,
  OWL_ERROR_BAD_PTR,
  OWL_ERROR_OUT_OF_BOUNDS,
  OWL_ERROR_OUTDATED_SWAPCHAIN,
  OWL_ERROR_INVALID_MEM_TYPE,
  OWL_ERROR_OUT_OF_SPACE,
  OWL_ERROR_NO_SUITABLE_DEVICE,
  OWL_ERROR_NO_SUITABLE_FORMAT,
  OWL_ERROR_UNKNOWN
};

struct owl_draw_cmd_vertex {
  OwlV3 pos;
  OwlV3 color;
  OwlV2 uv;
};

struct owl_draw_cmd_ubo {
  OwlM4 proj;
  OwlM4 view;
  OwlM4 model;
};

enum owl_draw_cmd_type {
  OWL_DRAW_CMD_TYPE_BASIC, /**/
  OWL_DRAW_CMD_TYPE_QUAD
};

struct owl_draw_cmd_basic {
  struct owl_img const *img;
  struct owl_draw_cmd_ubo ubo;

  OwlU32 index_count;
  OwlU32 const *indices;

  OwlU32 vertex_count;
  struct owl_draw_cmd_vertex const *vertices;
};

struct owl_draw_cmd_quad {
  struct owl_img const *img;
  struct owl_draw_cmd_ubo ubo;
  struct owl_draw_cmd_vertex vertices[4];
};

union owl_draw_cmd_storage {
  struct owl_draw_cmd_basic as_basic;
  struct owl_draw_cmd_quad as_quad;
};

struct owl_draw_cmd {
  enum owl_draw_cmd_type type;
  union owl_draw_cmd_storage storage;
};

enum owl_code owl_submit_draw_cmd(struct owl_vk_renderer *renderer,
                                  struct owl_draw_cmd const *cmd);

/* font creation */
enum owl_code owl_create_font(struct owl_vk_renderer *renderer, int size,
                              char const *path, struct owl_font **font);

void owl_destroy_font(struct owl_vk_renderer *renderer, struct owl_font *font);

/* frame */
enum owl_code owl_begin_frame(struct owl_vk_renderer *renderer);
enum owl_code owl_end_frame(struct owl_vk_renderer *renderer);

/* img */
enum owl_sampler_type {
  OWL_SAMPLER_TYPE_LINEAR,
  OWL_SAMPLER_TYPE_CLAMP,
  OWL_SAMPLER_TYPE_COUNT
};

enum owl_sampler_filter {
  OWL_SAMPLER_FILTER_NEAREST,
  OWL_SAMPLER_FILTER_LINEAR
};

enum owl_sampler_mip_mode {
  OWL_SAMPLER_MIP_MODE_NEAREST,
  OWL_SAMPLER_MIP_MODE_LINEAR
};

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

enum owl_sampler_addr_mode {
  OWL_SAMPLER_ADDR_MODE_REPEAT,
  OWL_SAMPLER_ADDR_MODE_MIRRORED_REPEAT,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE,
  OWL_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER
};

struct owl_img_desc {
  int width;
  int height;
  enum owl_pixel_format format;
  enum owl_sampler_mip_mode mip_mode;
  enum owl_sampler_filter min_filter;
  enum owl_sampler_filter mag_filter;
  enum owl_sampler_addr_mode wrap_u;
  enum owl_sampler_addr_mode wrap_v;
  enum owl_sampler_addr_mode wrap_w;
};

enum owl_code owl_create_img(struct owl_vk_renderer *renderer,
                             struct owl_img_desc const *desc,
                             unsigned char const *data, struct owl_img **img);

enum owl_code owl_create_img_from_file(struct owl_vk_renderer *renderer,
                                       struct owl_img_desc *desc,
                                       char const *path, struct owl_img **img);

void owl_destroy_img(struct owl_vk_renderer const *renderer,
                     struct owl_img *img);

enum owl_pipeline_type {
  OWL_PIPELINE_TYPE_MAIN,
  OWL_PIPELINE_TYPE_WIRES,
  OWL_PIPELINE_TYPE_FONT,
  OWL_PIPELINE_TYPE_COUNT
};

#define OWL_PIPELINE_TYPE_NONE OWL_PIPELINE_TYPE_COUNT

enum owl_code owl_bind_pipeline(struct owl_vk_renderer *renderer,
                                enum owl_pipeline_type type);

/* renderer */
enum owl_code owl_create_renderer(struct owl_window *window,
                                  struct owl_vk_renderer **renderer);

enum owl_code owl_recreate_swapchain(struct owl_window *window,
                                     struct owl_vk_renderer *renderer);

void owl_destroy_renderer(struct owl_vk_renderer *renderer);

/* math */
#define OWL_DEG_TO_RAD(angle) ((angle)*0.01745329252F)
#define OWL_RAD_TO_DEG(angle) ((angle)*57.2957795131F)

#define OWL_ZERO_V2(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
  } while (0)

#define OWL_ZERO_V3(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
    (v)[2] = 0.0F;                                                             \
  } while (0)

#define OWL_ZERO_V4(v)                                                         \
  do {                                                                         \
    (v)[0] = 0.0F;                                                             \
    (v)[1] = 0.0F;                                                             \
    (v)[2] = 0.0F;                                                             \
    (v)[3] = 0.0F;                                                             \
  } while (0)

#define OWL_SET_V2(x, y, out)                                                  \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
  } while (0)

#define OWL_SET_V3(x, y, z, out)                                               \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
    (out)[2] = (z);                                                            \
  } while (0)

#define OWL_SET_V4(x, y, z, w, out)                                            \
  do {                                                                         \
    (out)[0] = (x);                                                            \
    (out)[1] = (y);                                                            \
    (out)[2] = (z);                                                            \
    (out)[3] = (w);                                                            \
  } while (0)

#define OWL_IDENTITY_M4(m)                                                     \
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

#define OWL_COPY_V2(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
  } while (0)

#define OWL_COPY_V3(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
    (out)[2] = (in)[2];                                                        \
  } while (0)

#define OWL_COPY_V4(in, out)                                                   \
  do {                                                                         \
    (out)[0] = (in)[0];                                                        \
    (out)[1] = (in)[1];                                                        \
    (out)[2] = (in)[2];                                                        \
    (out)[3] = (in)[3];                                                        \
  } while (0)

#define OWL_COPY_M4(in, out)                                                   \
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

#define OWL_COPY_M3(in, out)                                                   \
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

#define OWL_DOT_V2(lhs, rhs) ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1])
#define OWL_DOT_V3(lhs, rhs)                                                   \
  ((lhs)[0] * (rhs)[0] + (lhs)[1] * (rhs)[1] + (lhs)[2] * (rhs)[2])

#define OWL_SCALE_V3(v, scale, out)                                            \
  do {                                                                         \
    (out)[0] = (scale) * (v)[0];                                               \
    (out)[1] = (scale) * (v)[1];                                               \
    (out)[2] = (scale) * (v)[2];                                               \
  } while (0)

#define OWL_SCALE_V4(v, scale, out)                                            \
  do {                                                                         \
    (out)[0] = (scale) * (v)[0];                                               \
    (out)[1] = (scale) * (v)[1];                                               \
    (out)[2] = (scale) * (v)[2];                                               \
    (out)[3] = (scale) * (v)[3];                                               \
  } while (0)

#define OWL_INV_SCALE_V2(v, scale, out)                                        \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
  } while (0)

#define OWL_INV_SCALE_V3(v, scale, out)                                        \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
    (out)[2] = (v)[2] / (scale);                                               \
  } while (0)

#define OWL_INV_SCALE_V4(v, scale, out)                                        \
  do {                                                                         \
    (out)[0] = (v)[0] / (scale);                                               \
    (out)[1] = (v)[1] / (scale);                                               \
    (out)[2] = (v)[2] / (scale);                                               \
    (out)[3] = (v)[3] / (scale);                                               \
  } while (0)

#define OWL_NEGATE_V3(v, out)                                                  \
  do {                                                                         \
    (out)[0] = -(v)[0];                                                        \
    (out)[1] = -(v)[1];                                                        \
    (out)[2] = -(v)[2];                                                        \
  } while (0)

#define OWL_NEGATE_V4(v, out)                                                  \
  do {                                                                         \
    (out)[0] = -(v)[0];                                                        \
    (out)[1] = -(v)[1];                                                        \
    (out)[2] = -(v)[2];                                                        \
    (out)[3] = -(v)[4];                                                        \
  } while (0)

#define OWL_ADD_V2(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
  } while (0)

#define OWL_ADD_V3(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
    (out)[2] = (lhs)[2] + (rhs)[2];                                            \
  } while (0)

#define OWL_ADD_V4(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] + (rhs)[0];                                            \
    (out)[1] = (lhs)[1] + (rhs)[1];                                            \
    (out)[2] = (lhs)[2] + (rhs)[2];                                            \
    (out)[3] = (lhs)[3] + (rhs)[3];                                            \
  } while (0)

#define OWL_SUB_V2(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
  } while (0)

#define OWL_SUB_V3(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
    (out)[2] = (lhs)[2] - (rhs)[2];                                            \
  } while (0)

#define OWL_SUB_V4(lhs, rhs, out)                                              \
  do {                                                                         \
    (out)[0] = (lhs)[0] - (rhs)[0];                                            \
    (out)[1] = (lhs)[1] - (rhs)[1];                                            \
    (out)[2] = (lhs)[2] - (rhs)[2];                                            \
    (out)[3] = (lhs)[3] - (rhs)[3];                                            \
  } while (0)

void owl_cross_v3(OwlV3 const lhs, OwlV3 const rhs, OwlV3 out);

void owl_mul_m4_v4(OwlM4 const m, OwlV4 const v, OwlV4 out);

float owl_mag_v2(OwlV2 const v);

float owl_mag_v3(OwlV3 const v);

void owl_norm_v3(OwlV3 const v, OwlV3 out);

void owl_make_rotate_m4(float angle, OwlV3 const axis, OwlM4 out);

void owl_translate_m4(OwlV3 const v, OwlM4 out);

void owl_ortho_m4(float left, float right, float bottom, float top, float near,
                  float far, OwlM4 out);

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

void owl_mul_complex(OwlV2 const lhs, OwlV2 const rhs, OwlV2 out);

#ifndef NDEBUG
void owl_print_v2(OwlV2 const v);
void owl_print_v3(OwlV3 const v);
void owl_print_v4(OwlV4 const v);
void owl_print_m4(OwlM4 const m);
#endif

enum owl_btn_state {
  OWL_BUTTON_STATE_NONE,
  OWL_BUTTON_STATE_PRESS,
  OWL_BUTTON_STATE_RELEASE,
  OWL_BUTTON_STATE_REPEAT
};

enum owl_mouse_btn {
  OWL_MOUSE_BUTTON_LEFT,
  OWL_MOUSE_BUTTON_MIDDLE,
  OWL_MOUSE_BUTTON_RIGHT,
  OWL_MOUSE_BUTTON_COUNT
};

/* taken from GLFW/glfw3.h */
enum owl_keyboard_key {
  /* Printable keys */
  OWL_KEYBOARD_KEY_SPACE = 32,
  OWL_KEYBOARD_KEY_APOSTROPHE = 39,
  OWL_KEYBOARD_KEY_COMMA = 44,
  OWL_KEYBOARD_KEY_MINUS = 45,
  OWL_KEYBOARD_KEY_PERIOD = 46,
  OWL_KEYBOARD_KEY_SLASH = 47,
  OWL_KEYBOARD_KEY_0 = 48,
  OWL_KEYBOARD_KEY_1 = 49,
  OWL_KEYBOARD_KEY_2 = 50,
  OWL_KEYBOARD_KEY_3 = 51,
  OWL_KEYBOARD_KEY_4 = 52,
  OWL_KEYBOARD_KEY_5 = 53,
  OWL_KEYBOARD_KEY_6 = 54,
  OWL_KEYBOARD_KEY_7 = 55,
  OWL_KEYBOARD_KEY_8 = 56,
  OWL_KEYBOARD_KEY_9 = 57,
  OWL_KEYBOARD_KEY_SEMICOLON = 59,
  OWL_KEYBOARD_KEY_EQUAL = 61,
  OWL_KEYBOARD_KEY_A = 65,
  OWL_KEYBOARD_KEY_B = 66,
  OWL_KEYBOARD_KEY_C = 67,
  OWL_KEYBOARD_KEY_D = 68,
  OWL_KEYBOARD_KEY_E = 69,
  OWL_KEYBOARD_KEY_F = 70,
  OWL_KEYBOARD_KEY_G = 71,
  OWL_KEYBOARD_KEY_H = 72,
  OWL_KEYBOARD_KEY_I = 73,
  OWL_KEYBOARD_KEY_J = 74,
  OWL_KEYBOARD_KEY_K = 75,
  OWL_KEYBOARD_KEY_L = 76,
  OWL_KEYBOARD_KEY_M = 77,
  OWL_KEYBOARD_KEY_N = 78,
  OWL_KEYBOARD_KEY_O = 79,
  OWL_KEYBOARD_KEY_P = 80,
  OWL_KEYBOARD_KEY_Q = 81,
  OWL_KEYBOARD_KEY_R = 82,
  OWL_KEYBOARD_KEY_S = 83,
  OWL_KEYBOARD_KEY_T = 84,
  OWL_KEYBOARD_KEY_U = 85,
  OWL_KEYBOARD_KEY_V = 86,
  OWL_KEYBOARD_KEY_W = 87,
  OWL_KEYBOARD_KEY_X = 88,
  OWL_KEYBOARD_KEY_Y = 89,
  OWL_KEYBOARD_KEY_Z = 90,
  OWL_KEYBOARD_KEY_LEFT_BRACKET = 91 /* [ */,
  OWL_KEYBOARD_KEY_BACKSLASH = 92 /* \ */,
  OWL_KEYBOARD_KEY_RIGHT_BRACKET = 93 /* ] */,
  OWL_KEYBOARD_KEY_GRAVE_ACCENT = 96 /* ` */,
  OWL_KEYBOARD_KEY_WORLD_1 = 161 /* non-US #1 */,
  OWL_KEYBOARD_KEY_WORLD_2 = 162 /* non-US #2 */,
  /* Function keys */
  OWL_KEYBOARD_KEY_ESCAPE = 256,
  OWL_KEYBOARD_KEY_ENTER = 257,
  OWL_KEYBOARD_KEY_TAB = 258,
  OWL_KEYBOARD_KEY_BACKSPACE = 259,
  OWL_KEYBOARD_KEY_INSERT = 260,
  OWL_KEYBOARD_KEY_DELETE = 261,
  OWL_KEYBOARD_KEY_RIGHT = 262,
  OWL_KEYBOARD_KEY_LEFT = 263,
  OWL_KEYBOARD_KEY_DOWN = 264,
  OWL_KEYBOARD_KEY_UP = 265,
  OWL_KEYBOARD_KEY_PAGE_UP = 266,
  OWL_KEYBOARD_KEY_PAGE_DOWN = 267,
  OWL_KEYBOARD_KEY_HOME = 268,
  OWL_KEYBOARD_KEY_END = 269,
  OWL_KEYBOARD_KEY_CAPS_LOCK = 280,
  OWL_KEYBOARD_KEY_SCROLL_LOCK = 281,
  OWL_KEYBOARD_KEY_NUM_LOCK = 282,
  OWL_KEYBOARD_KEY_PRINT_SCREEN = 283,
  OWL_KEYBOARD_KEY_PAUSE = 284,
  OWL_KEYBOARD_KEY_F1 = 290,
  OWL_KEYBOARD_KEY_F2 = 291,
  OWL_KEYBOARD_KEY_F3 = 292,
  OWL_KEYBOARD_KEY_F4 = 293,
  OWL_KEYBOARD_KEY_F5 = 294,
  OWL_KEYBOARD_KEY_F6 = 295,
  OWL_KEYBOARD_KEY_F7 = 296,
  OWL_KEYBOARD_KEY_F8 = 297,
  OWL_KEYBOARD_KEY_F9 = 298,
  OWL_KEYBOARD_KEY_F10 = 299,
  OWL_KEYBOARD_KEY_F11 = 300,
  OWL_KEYBOARD_KEY_F12 = 301,
  OWL_KEYBOARD_KEY_F13 = 302,
  OWL_KEYBOARD_KEY_F14 = 303,
  OWL_KEYBOARD_KEY_F15 = 304,
  OWL_KEYBOARD_KEY_F16 = 305,
  OWL_KEYBOARD_KEY_F17 = 306,
  OWL_KEYBOARD_KEY_F18 = 307,
  OWL_KEYBOARD_KEY_F19 = 308,
  OWL_KEYBOARD_KEY_F20 = 309,
  OWL_KEYBOARD_KEY_F21 = 310,
  OWL_KEYBOARD_KEY_F22 = 311,
  OWL_KEYBOARD_KEY_F23 = 312,
  OWL_KEYBOARD_KEY_F24 = 313,
  OWL_KEYBOARD_KEY_F25 = 314,
  OWL_KEYBOARD_KEY_KP_0 = 320,
  OWL_KEYBOARD_KEY_KP_1 = 321,
  OWL_KEYBOARD_KEY_KP_2 = 322,
  OWL_KEYBOARD_KEY_KP_3 = 323,
  OWL_KEYBOARD_KEY_KP_4 = 324,
  OWL_KEYBOARD_KEY_KP_5 = 325,
  OWL_KEYBOARD_KEY_KP_6 = 326,
  OWL_KEYBOARD_KEY_KP_7 = 327,
  OWL_KEYBOARD_KEY_KP_8 = 328,
  OWL_KEYBOARD_KEY_KP_9 = 329,
  OWL_KEYBOARD_KEY_KP_DECIMAL = 330,
  OWL_KEYBOARD_KEY_KP_DIVIDE = 331,
  OWL_KEYBOARD_KEY_KP_MULTIPLY = 332,
  OWL_KEYBOARD_KEY_KP_SUBTRACT = 333,
  OWL_KEYBOARD_KEY_KP_ADD = 334,
  OWL_KEYBOARD_KEY_KP_ENTER = 335,
  OWL_KEYBOARD_KEY_KP_EQUAL = 336,
  OWL_KEYBOARD_KEY_LEFT_SHIFT = 340,
  OWL_KEYBOARD_KEY_LEFT_CONTROL = 341,
  OWL_KEYBOARD_KEY_LEFT_ALT = 342,
  OWL_KEYBOARD_KEY_LEFT_SUPER = 343,
  OWL_KEYBOARD_KEY_RIGHT_SHIFT = 344,
  OWL_KEYBOARD_KEY_RIGHT_CONTROL = 345,
  OWL_KEYBOARD_KEY_RIGHT_ALT = 346,
  OWL_KEYBOARD_KEY_RIGHT_SUPER = 347,
  OWL_KEYBOARD_KEY_MENU = 348
};

#define OWL_KEYBOARD_KEY_LAST OWL_KEYBOARD_KEY_MENU

struct owl_input_state {
  double past_time;
  double cur_time;
  double dt_time;
  OwlV2 cur_cursor_pos;
  OwlV2 prev_cursor_pos;
  enum owl_btn_state mouse[OWL_MOUSE_BUTTON_COUNT];
  enum owl_btn_state keyboard[OWL_KEYBOARD_KEY_LAST];
};

struct owl_text_cmd {
  char const *text;
  struct owl_font const *font;
  OwlV2 pos;
  OwlV3 color;
};

/* text rendering */
enum owl_code owl_submit_text_cmd(struct owl_vk_renderer *renderer,
                                  struct owl_text_cmd const *cmd);

struct owl_window_desc {
  int width;
  int height;
  char const *title;
};

/* window creation */
enum owl_code owl_create_window(struct owl_window_desc const *desc,
                                struct owl_input_state **input,
                                struct owl_window **window);

void owl_destroy_window(struct owl_window *window);

void owl_poll_window_input(struct owl_window *window);

int owl_is_window_done(struct owl_window *window);

#endif

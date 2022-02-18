#ifndef OWL_WINDOW_H_
#define OWL_WINDOW_H_

#include "types.h"

struct owl_renderer_info;

enum owl_button_state {
  OWL_BUTTON_STATE_NONE,
  OWL_BUTTON_STATE_PRESS,
  OWL_BUTTON_STATE_RELEASE,
  OWL_BUTTON_STATE_REPEAT
};

enum owl_mouse_button {
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
  double dt_time;
  double past_time;
  double time;
  owl_v2 cursor_position;
  owl_v2 prev_cursor_position;
  enum owl_button_state mouse[OWL_MOUSE_BUTTON_COUNT];
  enum owl_button_state keyboard[OWL_KEYBOARD_KEY_LAST];
};

struct owl_window_info {
  int width;
  int height;
  char const *title;
};

struct owl_window {
  char const *title;

  void *data;

  int window_width;
  int window_height;
  int framebuffer_width;
  int framebuffer_height;

  struct owl_input_state input;
};

enum owl_code owl_window_init(struct owl_window_info const *info,
                              struct owl_input_state **input,
                              struct owl_window *w);

void owl_window_deinit(struct owl_window *w);

enum owl_code owl_window_fill_renderer_info(struct owl_window const *w,
                                               struct owl_renderer_info *info);

enum owl_code owl_window_create(struct owl_window_info const *info,
                                struct owl_input_state **input,
                                struct owl_window **w);

void owl_window_destroy(struct owl_window *w);

void owl_window_poll(struct owl_window *w);

int owl_window_is_done(struct owl_window *w);

#endif

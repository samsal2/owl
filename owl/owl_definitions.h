#ifndef OWL_DEFINITIONS_H
#define OWL_DEFINITIONS_H

#include <stdint.h>

#if defined(__cplusplus)
#define OWL_BEGIN_DECLARATIONS extern "C" {
#define OWL_END_DECLARATIONS }
#else
#define OWL_BEGIN_DECLARATIONS
#define OWL_END_DECLARATIONS
#endif

OWL_BEGIN_DECLARATIONS

#define OWL_PUBLIC extern
#define OWL_GLOBAL static
#define OWL_PRIVATE static
#define OWL_LOCAL_PERSIST static

typedef float owl_v2[2];
typedef float owl_v3[3];
typedef float owl_v4[4];
typedef float owl_q4[4];

typedef int owl_v2i[2];
typedef int owl_v3i[3];
typedef int owl_v4i[4];

typedef owl_v2 owl_m2[2];
typedef owl_v3 owl_m3[3];
typedef owl_v4 owl_m4[4];

struct owl_p_vertex {
  owl_v3 position;
};

struct owl_pcu_vertex {
  owl_v3 position;
  owl_v3 color;
  owl_v2 uv;
};

struct owl_pnuujw_vertex {
  owl_v3 position;
  owl_v3 normal;
  owl_v2 uv0;
  owl_v2 uv1;
  owl_v4 joints0;
  owl_v4 weights0;
};

struct owl_pvm_uniform {
  owl_m4 projection;
  owl_m4 view;
  owl_m4 model;
};

typedef int owl_code;

#define OWL_OK 0
#define OWL_ERROR_FATAL -1
#define OWL_ERROR_NO_FRAME_MEMORY -2
#define OWL_ERROR_NO_UPLOAD_MEMORY -3
#define OWL_ERROR_NO_MEMORY -4
#define OWL_ERROR_NOT_FOUND -5
#define OWL_ERROR_NO_SPACE -6
#define OWL_ERROR_INVALID_VALUE -7

OWL_END_DECLARATIONS

#endif

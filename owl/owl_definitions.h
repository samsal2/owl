#ifndef OWL_DEFINITIONS_H_
#define OWL_DEFINITIONS_H_

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
/** everything is ok */
#define OWL_OK 0
/** an internal fatal error ocurred */
#define OWL_ERROR_FATAL -1
/** couldn't obtain memory from the frame heap */
#define OWL_ERROR_NO_FRAME_MEMORY -2
/** couldn't obtain memory from the upload heap */
#define OWL_ERROR_NO_UPLOAD_MEMORY -3
/** malloc, calloc or realloc failed */
#define OWL_ERROR_NO_MEMORY -4
/** malloc, calloc or realloc failed */
#define OWL_ERROR_NOT_FOUND -5
/** a fixed size array ran out of space */
#define OWL_ERROR_NO_SPACE -6

OWL_END_DECLARATIONS

#endif

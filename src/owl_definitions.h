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

#define OWLAPI extern

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

struct owl_skybox_vertex {
	owl_v3 position;
};

struct owl_common_vertex {
	owl_v3 position;
	owl_v3 color;
	owl_v2 uv;
};

struct owl_common_uniform {
	owl_m4 projection;
	owl_m4 view;
	owl_m4 model;
};

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

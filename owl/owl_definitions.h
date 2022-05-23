#ifndef OWL_DEFINITIONS_H_
#define OWL_DEFINITIONS_H_

#include <stdint.h>

#if defined(__cplusplus)
#define OWL_BEGIN_DECLS extern "C" {
#define OWL_END_DECLS }
#else
#define OWL_BEGIN_DECLS
#define OWL_END_DECLS
#endif

OWL_BEGIN_DECLS

#define owl_public extern
#define owl_global static
#define owl_private static
#define owl_local_persist static

typedef uint8_t owl_u8;
typedef uint16_t owl_u16;
typedef uint32_t owl_u32;
typedef uint64_t owl_u64;

typedef int8_t owl_i8;
typedef int16_t owl_i16;
typedef int32_t owl_i32;
typedef int64_t owl_i64;

typedef int32_t owl_b32;

typedef unsigned char owl_byte;

typedef float owl_v2[2];
typedef float owl_v3[3];
typedef float owl_v4[4];

typedef float owl_q4[4];

typedef owl_i32 owl_v2i[2];
typedef owl_i32 owl_v3i[3];
typedef owl_i32 owl_v4i[4];

typedef owl_v2 owl_m2[2];
typedef owl_v3 owl_m3[3];
typedef owl_v4 owl_m4[4];

enum owl_code {
  OWL_SUCCESS,
  OWL_ERROR_BAD_ALLOCATION,
  OWL_ERROR_BAD_INIT,
  OWL_ERROR_BAD_HANDLE,
  OWL_ERROR_BAD_PTR,
  OWL_ERROR_OUT_OF_BOUNDS,
  OWL_ERROR_OUTDATED_SWAPCHAIN,
  OWL_ERROR_INVALID_MEMORY_TYPE,
  OWL_ERROR_OUT_OF_SPACE,
  OWL_ERROR_NO_SUITABLE_DEVICE,
  OWL_ERROR_NO_SUITABLE_FORMAT,
  OWL_ERROR_UNKNOWN
};

#define OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT 2

OWL_END_DECLS

#endif

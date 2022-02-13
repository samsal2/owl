#ifndef OWL_TYPES_H_
#define OWL_TYPES_H_

#include <stdint.h>

typedef uint8_t owl_u8;
typedef uint16_t owl_u16;
typedef uint32_t owl_u32;
typedef uint64_t owl_u64;

typedef int8_t owl_i8;
typedef int16_t owl_i16;
typedef int32_t owl_i32;
typedef int64_t owl_i64;

typedef unsigned char owl_byte;

typedef float owl_v2[2];
typedef float owl_v3[3];
typedef float owl_v4[4];

typedef int owl_v2i[2];
typedef int owl_v3i[3];
typedef int owl_v4i[4];

typedef owl_v2 owl_m2[2];
typedef owl_v3 owl_m3[3];
typedef owl_v4 owl_m4[4];

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

#endif

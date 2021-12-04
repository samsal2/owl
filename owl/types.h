#ifndef OWL_TYPES_H_
#define OWL_TYPES_H_

enum owl_code {
  OWL_SUCCESS, /**/
  OWL_ERROR_UNKNOWN
};

struct owl_extent {
  int width;
  int height;
};

enum owl_pixel_format {
  OWL_PIXEL_FORMAT_R8_UNORM,
  OWL_PIXEL_FORMAT_R8G8B8A8_SRGB
};

#include <stdint.h>
typedef uint32_t OwlU32;
typedef uint64_t OwlU64;
typedef unsigned char OwlByte;
typedef int OwlTexture;

typedef float OwlV2[2];
typedef float OwlV3[3];
typedef float OwlV4[4];

typedef OwlV3 OtterM3[3];
typedef OwlV4 OwlM4[4];

#ifndef NULL
#define NULL (void *)0
#endif

#endif

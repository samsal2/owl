#ifndef OWL_TYPES_H_
#define OWL_TYPES_H_

struct owl_extent {
  int width;
  int height;
};

#include <stdint.h>
typedef uint8_t OwlU8;
typedef uint32_t OwlU32;
typedef uint64_t OwlU64;
typedef unsigned char OwlByte;
typedef int OwlTexture;

typedef float OwlV2[2];
typedef float OwlV3[3];
typedef float OwlV4[4];

typedef int OwlV2I[2];
typedef int OwlV3I[3];

typedef OwlV3 OwlM3[3];
typedef OwlV4 OwlM4[4];

#ifndef NULL
#define NULL (void *)0
#endif

#endif

#ifndef OWL_INTERNAL_H_
#define OWL_INTERNAL_H_

#define OWL_GLOBAL static
#define OWL_INTERNAL static
#define OWL_LOCAL_PERSIST static
#define OWL_UNUSED(e) ((void)(e))

#include <assert.h>
#define OWL_ASSERT(e) assert(e)

#include <string.h>
#define OWL_MEMSET memset
#define OWL_MEMCPY memcpy

#include <stdlib.h>
#define OWL_MALLOC malloc
#define OWL_FREE free

#define OWL_ARRAY_SIZE(a) ((long)(sizeof((a)) / sizeof((a)[0])))
#define OWL_CLAMP(v, l, h) (((v) < (l)) ? (l) : (((v) > (h)) ? (h) : (v)))
#define OWL_MAX(a, b) (((a) < (b)) ? (b) : (a))

#endif

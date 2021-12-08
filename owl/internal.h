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

#ifndef NDEBUG

void *owl_dbg_malloc_(size_t s, char const *f, int l);
void *owl_dbg_realloc_(void *p, size_t s, char const *f, int l);
void owl_dbg_free_(void *p, char const *f, int l);

#define OWL_MALLOC(s) owl_dbg_malloc_(s, __FILE__, __LINE__)
#define OWL_REALLOC(p, s) owl_dbg_realloc_(p, s, __FILE__, __LINE__)
#define OWL_FREE(p) owl_dbg_free_(p, __FILE__, __LINE__)

#else

#include <stdlib.h>
#define OWL_MALLOC malloc
#define OWL_REALLOC realloc
#define OWL_FREE free

#endif

#define OWL_ARRAY_SIZE(a) ((long)(sizeof((a)) / sizeof((a)[0])))
#define OWL_CLAMP(v, l, h) (((v) < (l)) ? (l) : (((v) > (h)) ? (h) : (v)))
#define OWL_MAX(a, b) (((a) < (b)) ? (b) : (a))

#endif

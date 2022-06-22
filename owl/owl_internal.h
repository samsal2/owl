#ifndef OWL_INTERNAL_H_
#define OWL_INTERNAL_H_

#include <assert.h>
#define OWL_ASSERT(e) assert(e)

#include <string.h>
#define OWL_MEMSET(dst, c, s) memset(dst, c, s)
#define OWL_MEMCPY(dst, src, s) memcpy(dst, src, s)
#define OWL_STRNCPY(dst, src, n) strncpy(dst, src, n)
#define OWL_STRNCMP(lhs, rhs, n) strncmp(lhs, rhs, n)
#define OWL_STRLEN(str) strlen(str)

#if !defined(NDEBUG)

#include <stdio.h>

#define OWL_MALLOC(s) owl_debug_malloc(s, __FILE__, __LINE__)
void *
owl_debug_malloc(size_t s, char const *f, int l);

#define OWL_CALLOC(c, s) owl_debug_calloc(c, s, __FILE__, __LINE__)
void *
owl_debug_calloc(size_t c, size_t s, char const *f, int l);

#define OWL_REALLOC(p, s) owl_debug_realloc(p, s, __FILE__, __LINE__)
void *
owl_debug_realloc(void *p, size_t s, char const *f, int l);

#define OWL_FREE(p) owl_debug_free(p, __FILE__, __LINE__)
void
owl_debug_free(void *p, char const *f, int l);

#define OWL_DEBUG_LOG(...) owl_debug_log(__FILE__, __LINE__, __VA_ARGS__)
void
owl_debug_log(char const *f, int l, char const *format, ...);

#else /* NDEBUG */

#include <stdlib.h>
#define OWL_MALLOC(s) malloc(s)
#define OWL_CALLOC(c, s) calloc(c, s)
#define OWL_REALLOC(p, s) realloc(p, s)
#define OWL_FREE(p) free(p)

#define OWL_DEBUG_LOG(...)

#endif /* NDEBUG */

#if defined(OWL_DEBUG_PARANOID)
#if defined(NDEBUG)
#error "Debug must be enabled for OWL_PARANOID_ASSERT"
#endif
#include <assert.h>
#define OWL_PARANOID_ASSERT(e) assert(e)
#else
#define OWL_PARANOID_ASSERT(e)
#endif

#define OWL_CLAMP(v, l, h) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define OWL_MAX(a, b) ((a) < (b) ? (b) : (a))
#define OWL_MIN(a, b) ((a) > (b) ? (b) : (a))
#define OWL_UNUSED(e) ((void)e)
#define OWL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define OWL_ALIGN_UP_2(v, a) (((v) + (a)-1) & ~((a)-1))
#define OWL_STATIC_ASSERT(e, msg) typedef char OWL_STATIC_ASSERT_[!!(e)]

#endif

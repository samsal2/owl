#ifndef OWL_INTERNAL_H_
#define OWL_INTERNAL_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

#include <assert.h>
#define owl_assert(e) assert (e)

#include <string.h>
#define owl_memset(dst, c, s)    memset (dst, c, s)
#define owl_memcpy(dst, src, s)  memcpy (dst, src, s)
#define owl_strncpy(dst, src, n) strncpy (dst, src, n)
#define owl_strncmp(lhs, rhs, n) strncmp (lhs, rhs, n)

#if !defined(NDEBUG)

#include <stdio.h>

#define owl_malloc(s) owl_debug_malloc (s, __FILE__, __LINE__)
void *
owl_debug_malloc (size_t s, char const *f, int l);

#define owl_calloc(c, s) owl_debug_calloc (c, s, __FILE__, __LINE__)
void *
owl_debug_calloc (size_t c, size_t s, char const *f, int l);

#define owl_realloc(p, s) owl_debug_realloc (p, s, __FILE__, __LINE__)
void *
owl_debug_realloc (void *p, size_t s, char const *f, int l);

#define owl_free(p) owl_debug_free (p, __FILE__, __LINE__)
void
owl_debug_free (void *p, char const *f, int l);

#define OWL_DEBUG_LOG(...) owl_debug_log (__FILE__, __LINE__, __VA_ARGS__)
void
owl_debug_log (char const *f, int l, char const *fmt, ...);

#else /* NDEBUG */

#include <stdlib.h>
#define owl_malloc(s)     malloc (s)
#define owl_calloc(c, s)  calloc (c, s)
#define owl_realloc(p, s) realloc (p, s)
#define owl_free(p)       free (p)

#define OWL_DEBUG_LOG(...)

#endif /* NDEBUG */

#define owl_clamp(v, l, h)        ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define owl_max(a, b)             ((a) < (b) ? (b) : (a))
#define owl_min(a, b)             ((a) > (b) ? (b) : (a))
#define owl_unused(e)             ((void)e)
#define owl_array_size(a)         (sizeof (a) / sizeof ((a)[0]))
#define owl_alignu2(v, a)         ((v) + (a)-1) & ~((a)-1)
#define owl_static_assert(e, msg) typedef char owl_static_assert_[!!(e)]

OWL_END_DECLS

#endif

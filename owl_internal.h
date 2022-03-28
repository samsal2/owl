#ifndef OWL_INTERNAL_H_
#define OWL_INTERNAL_H_

#define OWL_GLOBAL static
#define OWL_INTERNAL static
#define OWL_LOCAL_PERSIST static

#include <assert.h>
#define OWL_ASSERT(e) assert(e)

#include <string.h>
#define OWL_MEMSET(dst, c, s) memset(dst, c, s)
#define OWL_MEMCPY(dst, src, s) memcpy(dst, src, s)
#define OWL_STRNCPY(dst, src, n) strncpy(dst, src, n)
#define OWL_STRNCMP(lhs, rhs, n) strncmp(lhs, rhs, n)

#if !defined(NDEBUG)

#include <stdio.h>

#define OWL_MALLOC(s) owl_debug_malloc_(s, __FILE__, __LINE__)
void *owl_debug_malloc_(size_t s, char const *f, int l);

#define OWL_CALLOC(c, s) owl_debug_calloc_(c, s, __FILE__, __LINE__)
void *owl_debug_calloc_(size_t c, size_t s, char const *f, int l);

#define OWL_REALLOC(p, s) owl_debug_realloc_(p, s, __FILE__, __LINE__)
void *owl_debug_realloc_(void *p, size_t s, char const *f, int l);

#define OWL_FREE(p) owl_debug_free_(p, __FILE__, __LINE__)
void owl_debug_free_(void *p, char const *f, int l);

#define OWL_DEBUG_LOG(...) owl_debug_log_(__FILE__, __LINE__, __VA_ARGS__)
void owl_debug_log_(char const *f, int l, char const *fmt, ...);

#else /* NDEBUG */

#include <stdlib.h>
#define OWL_MALLOC(s) malloc(s)
#define OWL_CALLOC(c, s) calloc(c, s)
#define OWL_REALLOC(p, s) realloc(p, s)
#define OWL_FREE(p) free(p)

#define OWL_DEBUG_LOG(...)

#endif /* NDEBUG */

#define OWL_CLAMP(v, l, h) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))
#define OWL_MAX(a, b) ((a) < (b) ? (b) : (a))
#define OWL_MIN(a, b) ((a) > (b) ? (b) : (a))
#define OWL_UNUSED(e) ((void)e)
#define OWL_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define OWL_ALIGNU2(v, a) ((v) + (a)-1) & ~((a)-1)
#define OWL_STATIC_ASSERT(e, msg) typedef char owl_static_assert_[!!(e)]

#if !defined(NDEBUG)

#define OWL_VK_CHECK(e)                                                        \
  do {                                                                         \
    VkResult const result_ = e;                                                \
    if (VK_SUCCESS != result_)                                                 \
      OWL_DEBUG_LOG("OWL_VK_CHECK(%s) result = %i\n", #e, result_);            \
    OWL_ASSERT(VK_SUCCESS == result_);                                         \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

#if !defined(NDEBUG)

#define OWL_ENABLE_VALIDATION

#endif /* NDEBUG */

#endif

#ifndef OWL_VK_INTERNAL_H_
#define OWL_VK_INTERNAL_H_

#ifndef NDEBUG

#include <owl/internal.h>
#include <stdio.h>

#define OWL_VK_CHECK(e)                                                      \
  do {                                                                       \
    VkResult const result_ = e;                                              \
    if (VK_SUCCESS != result_)                                               \
      printf("OWL_VK_CHECK: error with code %i\n", result_);                 \
    OWL_ASSERT(VK_SUCCESS == result_);                                       \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

#define OWL_ALIGN(v, a) (v + a - 1) & ~(a - 1)

#endif

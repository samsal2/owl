#ifndef OWL_VECTOR_H_
#define OWL_VECTOR_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vector {
  uint64_t num;
  uint64_t cap;
  uint64_t esize;
  union {
    long double ld_;
    long long ll_;
    uint8_t data[1];
  } aligned;
};

#define owl_vector(T) T *

#define owl_vector_init(v, num, type) owl_vector_init_(v, num, sizeof(type))
OWL_PUBLIC owl_code
owl_vector_init_(void **v, uint64_t num, uint64_t esize);

#define owl_vector_deinit(v) owl_vector_deinit_(v)
OWL_PUBLIC void
owl_vector_deinit_(void **v);

OWL_END_DECLS

#endif

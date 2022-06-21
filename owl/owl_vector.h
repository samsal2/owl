#ifndef OWL_VECTOR_H_
#define OWL_VECTOR_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLARATIONS

union owl_vector_aligned_data {
  long long ll_;
  long double ld_;
  uint8_t data[1];
};

struct owl_vector {
  uint64_t size;
  uint64_t capacity;
  uint64_t element_size;
  union owl_vector_aligned_data aligned;
};

#define OWL_VECTOR(type) type *

#define OWL_VECTOR_INIT(vec, capacity, size)                                  \
  owl_vector_init_((void **)(vec), capacity, size, sizeof(**(vec)))

OWL_PUBLIC owl_code
owl_vector_init_(void **vec, uint64_t capacity, uint64_t size,
    uint64_t element_size);

#define OWL_VECTOR_SIZE(vec) owl_vector_size_((void *)(vec))
OWL_PUBLIC uint64_t
owl_vector_size_(void *vec);

#define OWL_VECTOR_CLEAR(vec) owl_vector_clear_((void *)(vec))
OWL_PUBLIC void
owl_vector_clear_(void *vec);

#define OWL_VECTOR_PUSH_BACK(vec, value)                                      \
  owl_vector_push_back_((void **)(vec), (void *)&(value), sizeof(value));
OWL_PUBLIC owl_code
owl_vector_push_back_(void **vec, void *value, uint64_t element_size);

#define OWL_VECTOR_POP(vec) owl_vector_pop_((void *)(vec))
OWL_PUBLIC void
owl_vector_pop_(void *vec);

#define OWL_VECTOR_DEINIT(vec) owl_vector_deinit_((void *)(vec))
OWL_PUBLIC void
owl_vector_deinit_(void *vec);

OWL_END_DECLARATIONS

#endif

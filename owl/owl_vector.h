#ifndef OWL_VECTOR_H_
#define OWL_VECTOR_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

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

#define owl_vector(type) type *

#define owl_vector_init(vec, capacity, size)                                  \
  owl_vector_init_((void **)(vec), capacity, size, sizeof(**(vec)))

owl_public owl_code
owl_vector_init_(void **vec, uint64_t capacity, uint64_t size,
                 uint64_t element_size);

#define owl_vector_size(vec) owl_vector_size_((void *)(vec))
owl_public uint64_t
owl_vector_size_(void *vec);

#define owl_vector_clear(vec) owl_vector_clear_((void *)(vec))
owl_public void
owl_vector_clear_(void *vec);

#define owl_vector_push_back(vec, value)                                      \
  owl_vector_push_back_((void **)(vec), (void *)&(value), sizeof(value));
owl_public owl_code
owl_vector_push_back_(void **vec, void *value, uint64_t element_size);

#define owl_vector_pop(vec) owl_vector_pop_((void *)(vec))
owl_public void
owl_vector_pop_(void *vec);

#define owl_vector_deinit(vec) owl_vector_deinit_((void *)(vec))
owl_public void
owl_vector_deinit_(void *vec);

OWL_END_DECLS

#endif

#include "owl_vector.h"

#include "owl_internal.h"

#include <stddef.h>

#define owl_vector_data(v) ((v)->aligned.data)

#define owl_container_of(ptr, member, type)                                   \
  (type *)(ptr - offsetof(type, member))

OWL_PUBLIC owl_code
owl_vector_init_(void **v, uint64_t num, uint64_t esize)
{
  struct owl_vector *vec;

  owl_code code = OWL_OK;

  vec = owl_malloc(num * esize);
  if (vec) {
    vec->num = num;
    vec->cap = num;
    vec->esize = esize;

    *v = vec->aligned.data;
  } else {
    code = OWL_ERROR_NO_MEMORY;
  }

  return code;
}

OWL_PUBLIC void
owl_vector_deinit_(void **v)
{
  uint8_t *data = *v;
  owl_free(owl_container_of(*data, aligned, struct owl_vector));
}

#include "owl_vector.h"

#include "owl_internal.h"

#include <stddef.h>

#define owl_vector_data(head) (&((head)->aligned.data[0]))

#define owl_container_of(ptr, member, type)                                   \
  ((type *)(((uint8_t *)(ptr)) - offsetof(type, member)))

#define owl_vector_head(vec)                                                  \
  owl_container_of(vec, aligned.data, struct owl_vector)

#define owl_vector_at(head, i)                                                \
  (&((uint8_t *)(&(head)->aligned.data[0]))[head->esize * (i)])

owl_public owl_code
owl_vector_init_(void **v, uint64_t cap, uint64_t size, uint64_t esize) {
  struct owl_vector *head;

  owl_code code = OWL_OK;

  if (cap < size)
    return OWL_ERROR_FATAL; /* not really fatal */

  /** FIXME(samuel): calculate the exact size instead */
  head = owl_malloc(cap * esize + sizeof(struct owl_vector));
  if (head) {
    head->size = size;
    head->cap = cap;
    head->esize = esize;

    *v = owl_vector_data(head);
  } else {
    code = OWL_ERROR_NO_MEMORY;
  }

  return code;
}
owl_public uint64_t
owl_vector_size_(void *vec) {
  return owl_vector_head(vec)->size;
}

owl_public void
owl_vector_clear_(void *vec) {
  owl_vector_head(vec)->size = 0;
}

owl_public owl_code
owl_vector_push_back_(void **vec, void *value, uint64_t esize) {
  struct owl_vector *head = owl_vector_head(*vec);

  if (esize != head->esize)
    return OWL_ERROR_FATAL; /* not really fatal, just dumb */

  if (head->cap <= head->size + 1) {
    uint64_t size;
    struct owl_vector *new_head;

    /** FIXME(samuel): calculate the exact size instead */
    size = head->esize * head->cap * 2 + sizeof(struct owl_vector);

    new_head = owl_realloc(head, size);
    if (!new_head)
      return OWL_ERROR_NO_MEMORY;

    head = new_head;

    head->cap = head->cap * 2;

    *vec = owl_vector_data(head);
  }

  owl_memcpy(owl_vector_at(head, head->size), value, head->esize);

  ++head->size;

  return OWL_OK;
}

owl_public void
owl_vector_pop_(void *vec) {
  struct owl_vector *head = owl_vector_head(vec);

  if (head->size)
    --head->size;
}

owl_public void
owl_vector_deinit_(void *v) {
  owl_free(owl_vector_head(v));
}

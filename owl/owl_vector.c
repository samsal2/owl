#include "owl_vector.h"

#include "owl_internal.h"

#include <stddef.h>

#define OWL_VECTOR_DATA(head) (&((head)->aligned.data[0]))

#define OWL_CONTAINER_OF(ptr, member, type)                                   \
  ((type *)(((uint8_t *)(ptr)) - offsetof(type, member)))

#define OWL_VECTOR_HEAD(vec)                                                  \
  OWL_CONTAINER_OF(vec, aligned.data, struct owl_vector)

#define OWL_VECTOR_AT(head, i)                                                \
  (&((uint8_t *)(&(head)->aligned.data[0]))[head->element_size * (i)])

OWL_PUBLIC owl_code
owl_vector_init_(void **vec, uint64_t capacity, uint64_t size,
                 uint64_t element_size) {
  struct owl_vector *head;

  owl_code code = OWL_OK;

  if (capacity < size)
    return OWL_ERROR_FATAL; /* not really fatal */

  /** FIXME(samuel): calculate the exact size instead */
  head = OWL_MALLOC(capacity * element_size + sizeof(struct owl_vector));
  if (head) {
    head->size = size;
    head->capacity = capacity;
    head->element_size = element_size;

    *vec = OWL_VECTOR_DATA(head);
  } else {
    code = OWL_ERROR_NO_MEMORY;
  }

  return code;
}
OWL_PUBLIC uint64_t
owl_vector_size_(void *vec) {
  return OWL_VECTOR_HEAD(vec)->size;
}

OWL_PUBLIC void
owl_vector_clear_(void *vec) {
  OWL_VECTOR_HEAD(vec)->size = 0;
}

OWL_PUBLIC owl_code
owl_vector_push_back_(void **vec, void *value, uint64_t element_size) {
  struct owl_vector *head = OWL_VECTOR_HEAD(*vec);

  if (element_size != head->element_size)
    return OWL_ERROR_FATAL; /* not really fatal, just dumb */

  if (head->capacity <= head->size + 1) {
    uint64_t size;
    struct owl_vector *new_head;

    /** FIXME(samuel): calculate the exact size instead */
    size = head->element_size * head->capacity * 2 + sizeof(struct owl_vector);

    new_head = OWL_REALLOC(head, size);
    if (!new_head)
      return OWL_ERROR_NO_MEMORY;

    head = new_head;

    head->capacity = head->capacity * 2;

    *vec = OWL_VECTOR_DATA(head);
  }

  OWL_MEMCPY(OWL_VECTOR_AT(head, head->size), value, head->element_size);

  ++head->size;

  return OWL_OK;
}

OWL_PUBLIC void
owl_vector_pop_(void *vec) {
  struct owl_vector *head = OWL_VECTOR_HEAD(vec);

  if (head->size)
    --head->size;
}

OWL_PUBLIC void
owl_vector_deinit_(void *v) {
  OWL_FREE(OWL_VECTOR_HEAD(v));
}

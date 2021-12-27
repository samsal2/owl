#ifndef OWL_SLAB_ARENA_H_
#define OWL_SLAB_ARENA_H_

#include <owl/types.h>

#define OWL_SLAB_ARENA_SIZE 4096 * 1024 // 4 MB

union owl_slab_arena_storage {
  unsigned char data[OWL_SLAB_ARENA_SIZE];

  long double long_double_alignment_;
  long long long_long_alignment_;
};

struct owl_slab_arena {
  int pos;
  int owners;
  union owl_slab_arena_storage storage;
};

void owl_init_slab_arena(struct owl_slab_arena *arena);
void *owl_alloc_from_arena(struct owl_slab_arena *arena, int size);
void owl_free_from_arena(struct owl_slab_arena *arena, void *data);
void owl_deinit_slab_arena(struct owl_slab_arena *arena);

#endif

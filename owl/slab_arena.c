#include <owl/internal.h>
#include <owl/slab_arena.h>

OWL_INTERNAL int owl_align_size_(int size, int alignment) {
  int mod = size % alignment;
  return mod ? size - mod + alignment : size;
}

void owl_init_slab_arena(struct owl_slab_arena *arena) {
  arena->pos = 0;
  arena->owners = 0;
  arena->storage.data[0] = 0;
}

void *owl_alloc_from_arena(struct owl_slab_arena *arena, int size) {
  void *data = &arena->storage.data[arena->pos];

  // FIXME(samuel): test against aligned pos
  if ((arena->pos + size) >= OWL_SLAB_ARENA_SIZE)
    return NULL;

  ++arena->owners;
  arena->pos = owl_align_size_(arena->pos + size, sizeof(void *));
  
  return data;
}

void owl_free_from_arena(struct owl_slab_arena *arena, void *data) {
  OWL_UNUSED(data);

  if (arena->owners)
    --arena->owners;

  if (!arena->owners)
      arena->pos = 0;
}

void owl_deinit_slab_arena(struct owl_slab_arena *arena) {
  OWL_ASSERT(!arena->owners);

  arena->pos = 0;
  arena->owners = 0;
}

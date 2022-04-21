#include "owl_internal.h"
#include "owl_types.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void *owl_debug_malloc_(size_t s, char const *f, int l) {
  void *p = malloc(s);
  printf("\033[33m[OWL_MALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "s:%lu\n",
         f, l, p, s);
  return p;
}

void *owl_debug_calloc_(size_t c, size_t s, char const *f, int l) {
  void *p = calloc(c, s);
  printf("\033[33m[OWL_CALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "c:%lu s:%lu\n",
         f, l, p, c, s);
  return p;
}

void *owl_debug_realloc_(void *p, size_t s, char const *f, int l) {
  void *np = realloc(p, s);
  printf("\033[33m[OWL_REALLOC]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p "
         "np:%p s:%lu\n",
         f, l, p, np, s);
  return np;
}

void owl_debug_free_(void *p, char const *f, int l) {
  printf("\033[33m[OWL_FREE]\033[0m \033[31m(f:%s l:%d)\033[0m p:%p\n", f, l,
         p);
  free(p);
}

void owl_debug_log_(char const *f, int l, char const *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf("\033[33m[OWL_DEBUG_LOG]\033[0m \033[31m(f:%s l:%d)\033[0m ", f, l);
  vprintf(fmt, args);
  va_end(args);
}

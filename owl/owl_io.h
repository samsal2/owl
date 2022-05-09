#ifndef OWL_IO_H_
#define OWL_IO_H_

#include "owl_internal.h"

OWL_BEGIN_DECLS

struct owl_io {
  int empty_;
};

owl_public double
owl_io_time_stamp_get (void);

OWL_END_DECLS

#endif

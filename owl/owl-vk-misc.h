#ifndef OWL_VK_MISC_H_
#define OWL_VK_MISC_H_

#include "owl-definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

owl_public owl_u32
owl_vk_find_memory_type(struct owl_vk_renderer *vk, owl_u32 filter,
                        owl_u32 properties);

owl_public enum owl_code
owl_vk_begin_im_command_buffer(struct owl_vk_renderer *vk);

owl_public enum owl_code
owl_vk_end_im_command_buffer(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

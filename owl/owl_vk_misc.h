#ifndef OWL_VK_MISC_H_
#define OWL_VK_MISC_H_

#include "owl_definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;

owl_public uint32_t
owl_vk_find_memory_type(struct owl_vk_renderer *vk, uint32_t filter,
                        uint32_t properties);

owl_public owl_code
owl_vk_begin_im_command_buffer(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_end_im_command_buffer(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

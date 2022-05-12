#ifndef OWL_VK_FONT_H_
#define OWL_VK_FONT_H_

#include "owl_definitions.h"
#include "owl_glyph.h"
#include "owl_vk_image.h"

#include <vulkan/vulkan.h>

OWL_BEGIN_DECLS

struct owl_vk_context;
struct owl_vk_pipeline_manager;
struct owl_vk_stage_heap;

#define OWL_VK_FONT_FIRST_CHAR ((owl_i32)(' '))
#define OWL_VK_FONT_CHAR_COUNT ((owl_i32)('~' - ' '))
struct owl_vk_font {
  struct owl_vk_image    atlas;
  struct owl_packed_char chars[OWL_VK_FONT_CHAR_COUNT];
};

owl_public enum owl_code
owl_vk_font_init (struct owl_vk_font       *font,
                  struct owl_vk_context    *ctx,
                  struct owl_vk_stage_heap *heap,
                  char const               *path,
                  owl_i32                   sz);

owl_public void
owl_vk_font_deinit (struct owl_vk_font          *font,
                    struct owl_vk_context const *ctx);

owl_public enum owl_code
owl_vk_font_fill_glyph (struct owl_vk_font *font,
                        char                c,
                        owl_v2              offset,
                        struct owl_glyph   *glyph);

OWL_END_DECLS

#endif

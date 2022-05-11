#ifndef OWL_VK_RENDERER_H_
#define OWL_VK_RENDERER_H_

#include "owl_camera.h"
#include "owl_definitions.h"
#include "owl_vk_attachment.h"
#include "owl_vk_context.h"
#include "owl_vk_frame.h"
#include "owl_vk_garbage.h"
#include "owl_vk_pipeline_manager.h"
#include "owl_vk_stage_heap.h"
#include "owl_vk_swapchain.h"
#include "owl_vk_types.h"

OWL_BEGIN_DECLS

struct owl_window;

#define OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT 2

struct owl_vk_renderer {
  owl_i32 width;
  owl_i32 height;
  struct owl_camera camera;
  struct owl_vk_context context;
  struct owl_vk_attachment color_attachment;
  struct owl_vk_attachment depth_attachment;
  struct owl_vk_swapchain swapchain;
  struct owl_vk_pipeline_manager pipelines;
  struct owl_vk_garbage garbage;
  struct owl_vk_stage_heap stage_heap;

  struct owl_vk_font const *font;

  owl_i32 frame;
  struct owl_vk_frame frames[OWL_VK_RENDERER_IN_FLIGHT_FRAME_COUNT];
};

owl_public enum owl_code
owl_vk_renderer_init (struct owl_vk_renderer *vkr, struct owl_window *w);

owl_public void
owl_vk_renderer_deinit (struct owl_vk_renderer *vkr);

owl_public enum owl_code
owl_vk_renderer_resize (struct owl_vk_renderer *vkr, owl_i32 w, owl_i32 h);

owl_public void *
owl_vk_renderer_frame_heap_allocate (
    struct owl_vk_renderer *vkr, owl_u64 sz,
    struct owl_vk_frame_heap_allocation *allocation);

owl_public void *
owl_vk_renderer_stage_heap_allocate (
    struct owl_vk_renderer *vkr, owl_u64 sz,
    struct owl_vk_stage_heap_allocation *allocation);

owl_public void
owl_vk_renderer_stage_heap_free (struct owl_vk_renderer *vkr, void *p);

owl_public void
owl_vk_renderer_font_set (struct owl_vk_renderer *vkr,
                          struct owl_vk_font const *font);

owl_public enum owl_code
owl_vk_renderer_frame_begin (struct owl_vk_renderer *vkr);

owl_public enum owl_code
owl_vk_renderer_frame_end (struct owl_vk_renderer *vkr);

owl_public enum owl_code
owl_vk_renderer_pipeline_bind (struct owl_vk_renderer *vkr,
                               enum owl_pipeline_id id);

owl_public struct owl_vk_frame *
owl_vk_renderer_frame_get (struct owl_vk_renderer *vkr);

struct owl_quad;

owl_public enum owl_code
owl_vk_renderer_draw_quad (struct owl_vk_renderer *vkr,
                           struct owl_quad const *q, owl_m4 const matrix);

struct owl_glyph;

owl_public enum owl_code
owl_vk_renderer_draw_glyph (struct owl_vk_renderer *vkr,
                            struct owl_glyph const *glyph, owl_v3 const color);

owl_public enum owl_code
owl_vk_renderer_draw_text (struct owl_vk_renderer *vkr, char const *text,
                           owl_v3 const position, owl_v3 const color);

struct owl_model;

owl_public enum owl_code
owl_vk_renderer_draw_model (struct owl_vk_renderer *vkr,
                            struct owl_model const *model,
                            owl_m4 const matrix);
OWL_END_DECLS

#endif

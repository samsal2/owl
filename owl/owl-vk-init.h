#ifndef OWL_VK_INIT_H_
#define OWL_VK_INIT_H_

#include "owl-definitions.h"

OWL_BEGIN_DECLS

struct owl_vk_renderer;
struct owl_plataform;

owl_public owl_code
owl_vk_init_instance(struct owl_vk_renderer *vk,
                     struct owl_plataform *plataform);

owl_public void
owl_vk_deinit_instance(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_surface(struct owl_vk_renderer *vk,
                    struct owl_plataform *plataform);

owl_public void
owl_vk_deinit_surface(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_device(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_device(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_attachments(struct owl_vk_renderer *vk, uint32_t w, uint32_t h);

owl_public void
owl_vk_deinit_attachments(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_render_passes(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_render_passes(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_swapchain(struct owl_vk_renderer *vk, uint32_t w, uint32_t h);

owl_public void
owl_vk_deinit_swapchain(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_pools(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_pools(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_layouts(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_layouts(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_shaders(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_shaders(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_pipelines(struct owl_vk_renderer *vk, uint32_t w, uint32_t h);

owl_public void
owl_vk_deinit_pipelines(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_upload_heap(struct owl_vk_renderer *vk, uint64_t size);

owl_public void
owl_vk_deinit_upload_heap(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_frames(struct owl_vk_renderer *vk, uint64_t size);

owl_public void
owl_vk_deinit_frames(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_frame_heaps(struct owl_vk_renderer *vk, uint64_t size);

owl_public void
owl_vk_deinit_frame_heaps(struct owl_vk_renderer *vk);

owl_public owl_code
owl_vk_init_samplers(struct owl_vk_renderer *vk);

owl_public void
owl_vk_deinit_samplers(struct owl_vk_renderer *vk);

OWL_END_DECLS

#endif

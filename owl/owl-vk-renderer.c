#include "owl-vk-renderer.h"

#include "owl-internal.h"
#include "owl-plataform.h"
#include "owl-vector-math.h"
#include "owl-vk-font.h"
#include "owl-vk-init.h"
#include "owl-vk-skybox.h"

#define OWL_VK_DEFAULT_HEAP_SIZE (1 << 16)

owl_public enum owl_code
owl_vk_renderer_init(struct owl_vk_renderer *vk,
                     struct owl_plataform *plataform)
{
  owl_u32 width;
  owl_u32 height;

  owl_v3 eye;
  owl_v3 direction;
  owl_v3 up;
  enum owl_code code;

  float const fov = owl_deg2rad(45.0F);
  float const near = 0.01;
  float const far = 10.0F;

  owl_v3_set(eye, 0.0F, 0.0F, 5.0F);
  owl_v4_set(direction, 0.0F, 0.0F, 1.0F, 1.0F);
  owl_v3_set(up, 0.0F, 1.0F, 0.0F);

  owl_m4_perspective(fov, 1.0F, near, far, vk->projection);
  owl_m4_look(eye, direction, up, vk->view);

  vk->plataform = plataform;
  vk->im_command_buffer = VK_NULL_HANDLE;
  vk->skybox_loaded = 0;
  vk->font_loaded = 0;
  vk->pipeline = OWL_VK_PIPELINE_NONE;

  vk->swapchain_clear_values[0].color.float32[0] = 0.0F;
  vk->swapchain_clear_values[0].color.float32[1] = 0.0F;
  vk->swapchain_clear_values[0].color.float32[2] = 0.0F;
  vk->swapchain_clear_values[0].color.float32[3] = 1.0F;
  vk->swapchain_clear_values[1].depthStencil.depth = 1.0F;
  vk->swapchain_clear_values[1].depthStencil.stencil = 0.0F;

  owl_plataform_get_framebuffer_dimensions(plataform, &width, &height);
  vk->width = width;
  vk->height = height;

  code = owl_vk_init_instance(vk, plataform);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize instance!\n");
    goto out;
  }

  code = owl_vk_init_surface(vk, plataform);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize surface!\n");
    goto error_deinit_instance;
  }

  code = owl_vk_init_device(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize device");
    goto error_deinit_surface;
  }

  OWL_DEBUG_LOG("%u\n, %u\n", width, height);

  code = owl_vk_init_attachments(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize attachments");
    goto error_deinit_device;
  }

  code = owl_vk_init_render_passes(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize render passes!\n");
    goto error_deinit_attachments;
  }

  code = owl_vk_init_swapchain(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize swapchain!\n");
    goto error_deinit_render_passes;
  }

  code = owl_vk_init_pools(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize pools!\n");
    goto error_deinit_swapchain;
  }

  code = owl_vk_init_shaders(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize shaders!\n");
    goto error_deinit_pools;
  }

  code = owl_vk_init_layouts(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize layouts!\n");
    goto error_deinit_shaders;
  }

  code = owl_vk_init_pipelines(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize pipelines!\n");
    goto error_deinit_layouts;
  }

  code = owl_vk_init_upload_heap(vk, OWL_VK_DEFAULT_HEAP_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize upload heap!\n");
    goto error_deinit_pipelines;
  }

  code = owl_vk_init_samplers(vk);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize samplers!\n");
    goto error_deinit_upload_heap;
  }

  code = owl_vk_init_frames(vk, OWL_VK_DEFAULT_HEAP_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize frames!\n");
    goto error_deinit_samplers;
  }

  goto out;

error_deinit_samplers:
  owl_vk_deinit_samplers(vk);

error_deinit_upload_heap:
  owl_vk_deinit_upload_heap(vk);

error_deinit_pipelines:
  owl_vk_deinit_pipelines(vk);

error_deinit_layouts:
  owl_vk_deinit_layouts(vk);

error_deinit_shaders:
  owl_vk_deinit_shaders(vk);

error_deinit_pools:
  owl_vk_deinit_pools(vk);

error_deinit_swapchain:
  owl_vk_deinit_swapchain(vk);

error_deinit_render_passes:
  owl_vk_deinit_render_passes(vk);

error_deinit_attachments:
  owl_vk_deinit_attachments(vk);

error_deinit_device:
  owl_vk_deinit_device(vk);

error_deinit_surface:
  owl_vk_deinit_surface(vk);

error_deinit_instance:
  owl_vk_deinit_instance(vk);

out:
  return code;
}

owl_public void
owl_vk_renderer_deinit(struct owl_vk_renderer *vk)
{
  vkDeviceWaitIdle(vk->device);

  if (vk->font_loaded)
    owl_vk_font_unload(vk);

  if (vk->skybox_loaded)
    owl_vk_skybox_unload(vk);

  owl_vk_deinit_frames(vk);
  owl_vk_deinit_samplers(vk);
  owl_vk_deinit_upload_heap(vk);
  owl_vk_deinit_pipelines(vk);
  owl_vk_deinit_layouts(vk);
  owl_vk_deinit_shaders(vk);
  owl_vk_deinit_pools(vk);
  owl_vk_deinit_swapchain(vk);
  owl_vk_deinit_render_passes(vk);
  owl_vk_deinit_attachments(vk);
  owl_vk_deinit_device(vk);
  owl_vk_deinit_surface(vk);
  owl_vk_deinit_instance(vk);
}

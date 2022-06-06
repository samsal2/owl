#include "owl_vk_swapchain.h"

#include "owl_internal.h"
#include "owl_plataform.h"
#include "owl_vk_init.h"
#include "owl_vk_renderer.h"
#include "owl_vector_math.h"

owl_public owl_code
owl_vk_swapchain_resize(struct owl_vk_renderer *vk)
{
  uint32_t width;
  uint32_t height;

  float ratio;
  float const fov = owl_deg2rad(45.0F);
  float const near = 0.01;
  float const far = 10.0F;

  VkResult vk_result;
  owl_code code;

  vk_result = vkDeviceWaitIdle(vk->device);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  owl_plataform_get_framebuffer_dimensions(vk->plataform, &width, &height);
  vk->width = width;
  vk->height = height;

  ratio = (float)width / (float)height;
  owl_m4_perspective(fov, ratio, near, far, vk->projection);

  owl_vk_deinit_pipelines(vk);
  owl_vk_deinit_swapchain(vk);
  owl_vk_deinit_attachments(vk);

  code = owl_vk_init_attachments(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize attachments!\n");
    goto out;
  }

  code = owl_vk_init_swapchain(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize swapchain!\n");
    goto error_deinit_attachments;
  }

  code = owl_vk_init_pipelines(vk, width, height);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize pipelines!\n");
    goto error_deinit_swapchain;
  }

  goto out;

error_deinit_swapchain:
  owl_vk_deinit_swapchain(vk);

error_deinit_attachments:
  owl_vk_deinit_attachments(vk);

out:
  return code;
}

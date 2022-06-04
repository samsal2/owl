#include "owl-vk-swapchain.h"

#include "owl-internal.h"
#include "owl-plataform.h"
#include "owl-vk-init.h"
#include "owl-vk-renderer.h"

owl_public enum owl_code
owl_vk_swapchain_resize(struct owl_vk_renderer *vk)
{
  owl_u32 width;
  owl_u32 height;

  VkResult vk_result;
  enum owl_code code;

  vk_result = vkDeviceWaitIdle(vk->device);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  owl_plataform_get_framebuffer_dimensions(vk->plataform, &width, &height);
  vk->width = width;
  vk->height = height;

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

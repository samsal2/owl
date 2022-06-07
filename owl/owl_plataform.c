#include "owl_plataform.h"

#include "owl_internal.h"

#include "owl_vk_renderer.h"

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

owl_public owl_code
owl_plataform_init(struct owl_plataform *plataform, int w, int h,
                   char const *title) {
  int res;
  owl_code code = OWL_OK;

  res = glfwInit();
  if (res) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    plataform->opaque = glfwCreateWindow(w, h, title, NULL, NULL);

    if (plataform->opaque)
      plataform->title = title;
    else
      code = OWL_ERROR_FATAL;

  } else {
    code = OWL_ERROR_FATAL;
  }

  if (code)
    glfwTerminate();

  return code;
}

owl_public void
owl_plataform_deinit(struct owl_plataform *plataform) {
  glfwDestroyWindow(plataform->opaque);
  glfwTerminate();
}

#define OWL_MAX_INSTANCE_EXTENSIONS 64

owl_public owl_code
owl_plataform_get_vulkan_extensions(struct owl_plataform *plataform,
                                    uint32_t *num_extensions,
                                    char const *const **extensions) {
#if defined(OWL_ENABLE_VALIDATION)
  char const *const *tmp;
  owl_local_persist char const *names[OWL_MAX_INSTANCE_EXTENSIONS];

  owl_code code = OWL_OK;

  owl_unused(plataform);

  tmp = glfwGetRequiredInstanceExtensions(num_extensions);
  if (tmp && OWL_MAX_INSTANCE_EXTENSIONS > (*num_extensions + 1)) {
    owl_memcpy(names, tmp, *num_extensions * sizeof(*tmp));
    names[(*num_extensions)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    *extensions = names;
  } else {
    code = OWL_ERROR_NO_SPACE;
  }

  return code;

#else
  *extensions = glfwGetRequiredInstanceExtensions(num_extensions);
  if (!*extensions)
    return OWL_ERROR_FATAL;

  return OWL_OK;
#endif
}

owl_public int
owl_plataform_should_close(struct owl_plataform *plataform) {
  return glfwWindowShouldClose(plataform->opaque);
}

owl_public void
owl_plataform_poll_events(struct owl_plataform *plataform) {
  owl_unused(plataform);

  glfwPollEvents();
}

owl_public owl_code
owl_plataform_create_vulkan_surface(struct owl_plataform *plataform,
                                    struct owl_vk_renderer *vk) {
  VkResult vk_result;

  vk_result = glfwCreateWindowSurface(vk->instance, plataform->opaque, NULL,
                                      &vk->surface);
  OWL_DEBUG_LOG("vk_result: %i\n", vk_result);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_public void
owl_plataform_get_window_dimensions(struct owl_plataform const *plataform,
                                    uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetWindowSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

owl_public void
owl_plataform_get_framebuffer_dimensions(struct owl_plataform const *plataform,
                                         uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetFramebufferSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

owl_public double
owl_plataform_get_time(struct owl_plataform *plataform) {
  owl_unused(plataform);

  return glfwGetTime();
}

owl_public char const *
owl_plataform_get_title(struct owl_plataform const *plataform) {
  return plataform->title;
}

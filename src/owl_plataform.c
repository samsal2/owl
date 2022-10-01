#include "owl_plataform.h"

#include "owl_internal.h"

#include "owl_renderer.h"
#include "vulkan/vulkan_core.h"

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

#include <stdio.h>

OWLAPI int owl_plataform_init(struct owl_plataform *plataform, int w, int h,
                              char const *title) {
  int res;
  int ret = OWL_OK;

  res = glfwInit();
  if (res) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    plataform->opaque = glfwCreateWindow(w, h, title, NULL, NULL);
    if (plataform->opaque)
      plataform->title = title;
    else
      ret = OWL_ERROR_FATAL;

  } else {
    ret = OWL_ERROR_FATAL;
  }

  if (ret)
    glfwTerminate();

  return ret;
}

OWLAPI void owl_plataform_deinit(struct owl_plataform *plataform) {
  glfwDestroyWindow(plataform->opaque);
  glfwTerminate();
}

#define OWL_MAX_INSTANCE_EXTENSIONS 64

OWLAPI int
owl_plataform_get_required_instance_extensions(struct owl_plataform *plataform,
                                               uint32_t *num_extensions,
                                               char const *const **extensions) {
#if defined(OWL_ENABLE_VALIDATION)
  char const *const *tmp;
  static char const *names[OWL_MAX_INSTANCE_EXTENSIONS];

  int ret = OWL_OK;

  OWL_UNUSED(plataform);

  tmp = glfwGetRequiredInstanceExtensions(num_extensions);
  if (tmp && OWL_MAX_INSTANCE_EXTENSIONS > (*num_extensions + 2)) {
    OWL_MEMCPY(names, tmp, *num_extensions * sizeof(*tmp));
    names[(*num_extensions)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    *extensions = names;
  } else {
    ret = OWL_ERROR_NO_SPACE;
  }

  return ret;

#else
  OWL_UNUSED(plataform);

  *extensions = glfwGetRequiredInstanceExtensions(num_extensions);
  if (!*extensions)
    return OWL_ERROR_FATAL;

  return OWL_OK;
#endif
}

OWLAPI int owl_plataform_should_close(struct owl_plataform *plataform) {
  return glfwWindowShouldClose(plataform->opaque);
}

OWLAPI void owl_plataform_poll_events(struct owl_plataform *plataform) {
  OWL_UNUSED(plataform);

  glfwPollEvents();
}

OWLAPI int owl_plataform_create_vulkan_surface(struct owl_plataform *plataform,
                                               struct owl_renderer *r) {
  VkResult vk_result;

  vk_result = glfwCreateWindowSurface(r->instance, plataform->opaque, NULL,
                                      &r->surface);
  OWL_DEBUG_LOG("vk_result: %i\n", vk_result);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

OWLAPI void
owl_plataform_get_window_dimensions(struct owl_plataform const *plataform,
                                    uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetWindowSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

OWLAPI void
owl_plataform_get_framebuffer_dimensions(struct owl_plataform const *plataform,
                                         uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetFramebufferSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

OWLAPI double owl_plataform_get_time(struct owl_plataform *plataform) {
  OWL_UNUSED(plataform);

  return glfwGetTime();
}

OWLAPI char const *
owl_plataform_get_title(struct owl_plataform const *plataform) {
  return plataform->title;
}

OWLAPI int owl_plataform_load_file(char const *path,
                                   struct owl_plataform_file *file) {
  FILE *fp = NULL;
  int ret = OWL_OK;

  fp = fopen(path, "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    file->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file->data = OWL_MALLOC(file->size * sizeof(char));
    if (file->data)
      fread(file->data, file->size, 1, fp);
    else
      ret = OWL_ERROR_NO_MEMORY;

    fclose(fp);
  } else {
    ret = OWL_ERROR_NOT_FOUND;
  }

  file->path = path;

  return ret;
}

OWLAPI void owl_plataform_unload_file(struct owl_plataform_file *file) {
  OWL_FREE(file->data);
}

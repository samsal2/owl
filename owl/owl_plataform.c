#include "owl_plataform.h"

#include "owl_internal.h"

#include "owl_renderer.h"

/* clang-format off */
#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
/* clang-format on */

#include <stdio.h>

OWL_PUBLIC owl_code owl_plataform_init(struct owl_plataform *plataform, int w,
    int h, char const *title) {
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

OWL_PUBLIC void owl_plataform_deinit(struct owl_plataform *plataform) {
  glfwDestroyWindow(plataform->opaque);
  glfwTerminate();
}

#define OWL_MAX_INSTANCE_EXTENSION_COUNT 64

OWL_PUBLIC owl_code owl_plataform_get_required_instance_extensions(
    struct owl_plataform *plataform, uint32_t *extension_count,
    char const *const **extensions) {
#if defined(OWL_ENABLE_VALIDATION)
  char const *const *tmp;
  OWL_LOCAL_PERSIST char const *names[OWL_MAX_INSTANCE_EXTENSION_COUNT];

  owl_code code = OWL_OK;

  OWL_UNUSED(plataform);

  tmp = glfwGetRequiredInstanceExtensions(extension_count);
  if (tmp && OWL_MAX_INSTANCE_EXTENSION_COUNT > (*extension_count + 1)) {
    OWL_MEMCPY(names, tmp, *extension_count * sizeof(*tmp));
    names[(*extension_count)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    *extensions = names;
  } else {
    code = OWL_ERROR_NO_SPACE;
  }

  return code;

#else
  OWL_UNUSED(plataform);

  *extensions = glfwGetRequiredInstanceExtensions(extension_count);
  if (!*extensions)
    return OWL_ERROR_FATAL;

  return OWL_OK;
#endif
}

OWL_PUBLIC int owl_plataform_should_close(struct owl_plataform *plataform) {
  return glfwWindowShouldClose(plataform->opaque);
}

OWL_PUBLIC void owl_plataform_poll_events(struct owl_plataform *plataform) {
  OWL_UNUSED(plataform);

  glfwPollEvents();
}

OWL_PUBLIC owl_code owl_plataform_create_vulkan_surface(
    struct owl_plataform *plataform, struct owl_renderer *renderer) {
  VkResult vk_result;

  vk_result = glfwCreateWindowSurface(renderer->instance, plataform->opaque,
      NULL, &renderer->surface);
  OWL_DEBUG_LOG("vk_result: %i\n", vk_result);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

OWL_PUBLIC void owl_plataform_get_window_dimensions(
    struct owl_plataform const *plataform, uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetWindowSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

OWL_PUBLIC void owl_plataform_get_framebuffer_dimensions(
    struct owl_plataform const *plataform, uint32_t *width, uint32_t *height) {
  int internal_width;
  int internal_height;

  glfwGetFramebufferSize(plataform->opaque, &internal_width, &internal_height);

  *width = internal_width;
  *height = internal_height;
}

OWL_PUBLIC double owl_plataform_get_time(struct owl_plataform *plataform) {
  OWL_UNUSED(plataform);

  return glfwGetTime();
}

OWL_PUBLIC char const *owl_plataform_get_title(
    struct owl_plataform const *plataform) {
  return plataform->title;
}

OWL_PUBLIC owl_code owl_plataform_load_file(char const *path,
    struct owl_plataform_file *file) {
  FILE *fp = NULL;
  owl_code code = OWL_OK;

  fp = fopen(path, "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    file->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file->data = OWL_MALLOC(file->size * sizeof(char));
    if (file->data)
      fread(file->data, file->size, 1, fp);
    else
      code = OWL_ERROR_NO_MEMORY;

    fclose(fp);
  } else {
    code = OWL_ERROR_NOT_FOUND;
  }

  file->path = path;

  return code;
}

OWL_PUBLIC void owl_plataform_unload_file(struct owl_plataform_file *file) {
  OWL_FREE(file->data);
}

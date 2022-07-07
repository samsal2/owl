#include <owl/owl.h>

#include "internal.h"

#include <stdio.h>

#define OWL_VBO_SIZE (1 << 16)
#define OWL_IBO_SIZE (1 << 16)
#define OWL_UBO_SIZE (1 << 16)

#if defined(OWL_VK_VALIDATE) || 1
static VKAPI_ATTR VKAPI_CALL VkBool32
owl_gfx_dbg_cb(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
               VkDebugUtilsMessageTypeFlagsEXT type,
               VkDebugUtilsMessengerCallbackDataEXT const *data, void *user) {
  OWL_UNUSED(type);
  OWL_UNUSED(user);

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[34m(VERBOSE)\033[0m %s\n",
            data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(INFO)\033[0m %s\n",
            data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(WARNING)\033[0m %s\n",
            data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[31m(ERROR)\033[0m %s\n",
            data->pMessage);
  }

  return VK_FALSE;
}

static char const *const dbg_layers[] = {"VK_LAYER_KHRONOS_validation"};
PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;

#else

#endif

OWLAPI int owl_gfx_init(struct owl_gfx *gfx, struct owl_plataform *p) {
  VkApplicationInfo app_info;
  VkInstanceCreateInfo inst_info;
#if defined(OWL_VK_VALIDATION) || 1
  VkDebugUtilsMessengerCreateInfoEXT dbg_info;
#endif
  VkDeviceCreateInfo dev_info;

  int ret = OWL_OK;

  gfx->plataform = p;

  return OWL_OK;

error:
  return ret;
}

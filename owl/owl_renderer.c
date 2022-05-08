#include "owl_renderer.h"

#include "owl_draw_command.h"
#include "owl_internal.h"
#include "owl_io.h"
#include "owl_model.h"
#include "owl_types.h"
#include "owl_vector_math.h"
#include "owl_window.h"
#include "stb_image.h"
#include "stb_truetype.h"

#include <math.h>

#define OWL_MAX_PRESENT_MODES 16
#define OWL_UNRESTRICTED_DIMENSION (owl_u32) - 1
#define OWL_ACQUIRE_IMAGE_TIMEOUT (owl_u64) - 1
#define OWL_WAIT_FOR_FENCES_TIMEOUT (owl_u64) - 1
#define OWL_QUEUE_FAMILY_INDEX_NONE (owl_u32) - 1
#define OWL_ALL_SAMPLE_FLAG_BITS (owl_u32) - 1
#define OWL_JUST_ENOUGH_DESCRIPTORS 256

OWL_GLOBAL char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(OWL_ENABLE_VALIDATION)

OWL_GLOBAL char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

#endif /* OWL_ENABLE_VALIDATION */

#if !defined(NDEBUG)

#define OWL_VK_CHECK(e)                                                        \
  do {                                                                         \
    VkResult const result_ = e;                                                \
    if (VK_SUCCESS != result_) {                                               \
      OWL_DEBUG_LOG("OWL_VK_CHECK(%s) result = %i\n", #e, result_);            \
      OWL_ASSERT(0);                                                           \
    }                                                                          \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

OWL_INTERNAL enum owl_code owl_vk_result_as_owl_code(VkResult vkres) {
  switch ((int)vkres) {
  case VK_SUCCESS:
    return OWL_SUCCESS;

  default:
    return OWL_ERROR_UNKNOWN;
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_instance_init(struct owl_renderer_init_info const *info,
                           struct owl_renderer *r) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance_info;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = info->name;
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pNext = NULL;
  instance_info.flags = 0;
  instance_info.pApplicationInfo = &app;
#if defined(OWL_ENABLE_VALIDATION)
  instance_info.enabledLayerCount = OWL_ARRAY_SIZE(debug_validation_layers);
  instance_info.ppEnabledLayerNames = debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
  instance_info.enabledLayerCount = 0;
  instance_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
  instance_info.enabledExtensionCount = (owl_u32)info->instance_extension_count;
  instance_info.ppEnabledExtensionNames = info->instance_extensions;

  vkres = vkCreateInstance(&instance_info, NULL, &r->instance);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_instance_deinit(struct owl_renderer *r) {
  vkDestroyInstance(r->instance, NULL);
}

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32 owl_renderer_debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user_data) {
  struct owl_renderer const *renderer = user_data;

  OWL_UNUSED(type);
  OWL_UNUSED(renderer);

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

OWL_INTERNAL enum owl_code
owl_renderer_debug_messenger_init(struct owl_renderer *r) {
  VkDebugUtilsMessengerCreateInfoEXT info;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkCreateDebugUtilsMessengerEXT =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          r->instance, "vkCreateDebugUtilsMessengerEXT");

  OWL_ASSERT(vkCreateDebugUtilsMessengerEXT);

  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.pNext = NULL;
  info.flags = 0;
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = owl_renderer_debug_messenger_callback;
  info.pUserData = r;

  vkres = vkCreateDebugUtilsMessengerEXT(r->instance, &info, NULL,
                                         &r->debug_messenger);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_debug_messenger_deinit(struct owl_renderer *r) {
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

  vkDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          r->instance, "vkDestroyDebugUtilsMessengerEXT");

  OWL_ASSERT(vkDestroyDebugUtilsMessengerEXT);

  vkDestroyDebugUtilsMessengerEXT(r->instance, r->debug_messenger, NULL);
}

#endif /* OWL_ENABLE_VALIDATION */

OWL_INTERNAL enum owl_code
owl_renderer_surface_init(struct owl_renderer_init_info const *info,
                          struct owl_renderer *r) {
  return info->create_surface(r, info->surface_user_data, &r->surface);
}

OWL_INTERNAL void owl_renderer_surface_deinit(struct owl_renderer *r) {
  vkDestroySurfaceKHR(r->instance, r->surface, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_device_options_fill(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkres =
      vkEnumeratePhysicalDevices(r->instance, &r->device_option_count, NULL);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  if (OWL_RENDERER_MAX_DEVICE_OPTION_COUNT <= r->device_option_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto out;
  }

  vkres = vkEnumeratePhysicalDevices(r->instance, &r->device_option_count,
                                     r->device_options);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

OWL_INTERNAL
owl_b32 owl_renderer_query_families(struct owl_renderer const *r,
                                    owl_u32 *graphics_family_index,
                                    owl_u32 *present_family_index) {
  owl_i32 found;
  owl_i32 i;
  owl_u32 propertie_count;
  VkResult vkres = VK_SUCCESS;
  VkQueueFamilyProperties *properties;

  vkGetPhysicalDeviceQueueFamilyProperties(r->physical_device, &propertie_count,
                                           NULL);

  if (!(properties = OWL_MALLOC(propertie_count * sizeof(*properties)))) {
    found = 0;
    goto out;
  }

  vkGetPhysicalDeviceQueueFamilyProperties(r->physical_device, &propertie_count,
                                           properties);

  *graphics_family_index = OWL_QUEUE_FAMILY_INDEX_NONE;
  *present_family_index = OWL_QUEUE_FAMILY_INDEX_NONE;

  for (i = 0; i < (owl_i32)propertie_count; ++i) {
    VkBool32 has_surface;

    if (!properties[i].queueCount) {
      continue;
    }

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags) {
      *graphics_family_index = i;
    }

    vkres = vkGetPhysicalDeviceSurfaceSupportKHR(r->physical_device, i,
                                                 r->surface, &has_surface);

    if (OWL_SUCCESS != owl_vk_result_as_owl_code(vkres)) {
      found = 0;
      goto out_properties_free;
    }

    if (has_surface) {
      *present_family_index = i;
    }

    if (OWL_QUEUE_FAMILY_INDEX_NONE == *graphics_family_index) {
      continue;
    }

    if (OWL_QUEUE_FAMILY_INDEX_NONE == *present_family_index) {
      continue;
    }

    found = 1;
    goto out_properties_free;
  }

  found = 0;

out_properties_free:
  OWL_FREE(properties);

out:
  return found;
}

OWL_INTERNAL int
owl_validate_device_extensions(owl_u32 extension_count,
                               VkExtensionProperties const *extensions) {
  owl_i32 i;
  owl_i32 found = 1;
  owl_b32 extensions_found[OWL_ARRAY_SIZE(device_extensions)];


  for (i = 0; i < (owl_i32)OWL_ARRAY_SIZE(device_extensions); ++i) {
    extensions_found[i] = 0;
  }

  for (i = 0; i < (owl_i32)extension_count; ++i) {
    owl_u32 j;
    for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j) {
      if (!OWL_STRNCMP(device_extensions[j], extensions[i].extensionName,
                       VK_MAX_EXTENSION_NAME_SIZE)) {
        extensions_found[j] = 1;
      }
    }
  }

  for (i = 0; i < (owl_i32)OWL_ARRAY_SIZE(device_extensions); ++i) {
    if (!extensions_found[i]) {
      found = 0;
      goto out;
    }
  }

out:
  return found;
}

OWL_INTERNAL enum owl_code
owl_renderer_physical_device_select(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)r->device_option_count; ++i) {
    owl_u32 has_formats;
    owl_u32 has_modes;
    owl_u32 extension_count;
    VkPhysicalDeviceFeatures features;
    VkExtensionProperties *extensions;

    r->physical_device = r->device_options[i];

    vkres = vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface,
                                                 &has_formats, NULL);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }

    if (!has_formats) {
      continue;
    }

    vkres = vkGetPhysicalDeviceSurfacePresentModesKHR(
        r->physical_device, r->surface, &has_modes, NULL);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }

    if (!has_modes) {
      continue;
    }

    if (!owl_renderer_query_families(r, &r->graphics_queue_family,
                                     &r->present_queue_family)) {
      continue;
    }

    vkGetPhysicalDeviceFeatures(r->physical_device, &features);

    if (!features.samplerAnisotropy) {
      continue;
    }

    vkres = vkEnumerateDeviceExtensionProperties(r->physical_device, NULL,
                                                 &extension_count, NULL);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }

    if (!(extensions = OWL_MALLOC(extension_count * sizeof(*extensions)))) {
      code = OWL_ERROR_BAD_ALLOC;
      goto out;
    }

    vkres = vkEnumerateDeviceExtensionProperties(r->physical_device, NULL,
                                                 &extension_count, extensions);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      OWL_FREE(extensions);
      goto out;
    }

    if (!owl_validate_device_extensions(extension_count, extensions)) {
      OWL_FREE(extensions);
      continue;
    }

    OWL_FREE(extensions);

    code = OWL_SUCCESS;
    goto out;
  }

  code = OWL_ERROR_NO_SUITABLE_DEVICE;

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_surface_format_ensure(struct owl_renderer const *r) {
  owl_i32 i;
  owl_u32 format_count;
  VkSurfaceFormatKHR *formats;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkres = vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface,
                                               &format_count, NULL);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  if (!(formats = OWL_MALLOC(format_count * sizeof(*formats)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out;
  }

  vkres = vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface,
                                               &format_count, formats);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out_format_free;
  }

  for (i = 0; i < (owl_i32)format_count; ++i) {
    if (r->surface_format.format != formats[i].format) {
      continue;
    }

    if (r->surface_format.colorSpace != formats[i].colorSpace) {
      continue;
    }

    code = OWL_SUCCESS;
    goto out_format_free;
  }

  /* if the loop ends without finding a format */
  code = OWL_ERROR_NO_SUITABLE_FORMAT;

out_format_free:
  OWL_FREE(formats);

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_msaa_sampler_count_ensure(struct owl_renderer const *r) {
  VkPhysicalDeviceProperties properties;

  enum owl_code code = OWL_SUCCESS;
  VkSampleCountFlags limit = OWL_ALL_SAMPLE_FLAG_BITS;

  vkGetPhysicalDeviceProperties(r->physical_device, &properties);

  limit &= properties.limits.framebufferColorSampleCounts;
  limit &= properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_1_BIT & r->msaa_sample_count) {
    OWL_ASSERT(0 && "disabling multisampling is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (!(limit & r->msaa_sample_count)) {
    OWL_ASSERT(0 && "msaa_sample_count is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

OWL_INTERNAL enum owl_code owl_renderer_device_init(struct owl_renderer *r) {
  VkPhysicalDeviceFeatures features;
  VkDeviceCreateInfo device_info;
  VkDeviceQueueCreateInfo queue_infos[2];

  float const priority = 1.0F;
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_device_options_fill(r))) {
    goto out;
  }

  if (OWL_SUCCESS != (code = owl_renderer_physical_device_select(r))) {
    goto out;
  }

  vkGetPhysicalDeviceFeatures(r->physical_device, &features);

  r->surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
  r->surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  if (OWL_SUCCESS != (code = owl_renderer_surface_format_ensure(r))) {
    goto out;
  }

  /* TODO(samuel): add and option for choosing this */
  r->msaa_sample_count = VK_SAMPLE_COUNT_2_BIT;
  if (OWL_SUCCESS != (code = owl_renderer_msaa_sampler_count_ensure(r))) {
    goto out;
  }

  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = NULL;
  queue_infos[0].flags = 0;
  queue_infos[0].queueFamilyIndex = r->graphics_queue_family;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &priority;

  queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[1].pNext = NULL;
  queue_infos[1].flags = 0;
  queue_infos[1].queueFamilyIndex = r->present_queue_family;
  queue_infos[1].queueCount = 1;
  queue_infos[1].pQueuePriorities = &priority;

  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = NULL;
  device_info.flags = 0;
  if (r->graphics_queue_family == r->present_queue_family) {
    device_info.queueCreateInfoCount = 1;
  } else {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos;
  device_info.enabledLayerCount = 0;      /* deprecated */
  device_info.ppEnabledLayerNames = NULL; /* deprecated */
  device_info.enabledExtensionCount = OWL_ARRAY_SIZE(device_extensions);
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.pEnabledFeatures = &features;

  vkres = vkCreateDevice(r->physical_device, &device_info, NULL, &r->device);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  vkGetDeviceQueue(r->device, r->graphics_queue_family, 0, &r->graphics_queue);

  vkGetDeviceQueue(r->device, r->present_queue_family, 0, &r->present_queue);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_device_deinit(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

OWL_INTERNAL void owl_renderer_swapchain_extent_clamp(struct owl_renderer *r) {
  VkSurfaceCapabilitiesKHR capabilities;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      r->physical_device, r->surface, &capabilities));

  if (OWL_UNRESTRICTED_DIMENSION == capabilities.currentExtent.width) {
    r->swapchain_extent = capabilities.currentExtent;
  } else {
    owl_u32 const min_width = capabilities.minImageExtent.width;
    owl_u32 const min_height = capabilities.minImageExtent.height;
    owl_u32 const max_width = capabilities.maxImageExtent.width;
    owl_u32 const max_height = capabilities.maxImageExtent.height;

    r->swapchain_extent.width =
        OWL_CLAMP(r->swapchain_extent.width, min_width, max_width);

    r->swapchain_extent.height =
        OWL_CLAMP(r->swapchain_extent.height, min_height, max_height);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_present_mode_ensure(struct owl_renderer *r) {
  owl_i32 i;
  owl_u32 mode_count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  VkPresentModeKHR const preferred = r->swapchain_present_mode;

  vkres = vkGetPhysicalDeviceSurfacePresentModesKHR(
      r->physical_device, r->surface, &mode_count, NULL);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  if (OWL_MAX_PRESENT_MODES <= mode_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto out;
  }

  vkres = vkGetPhysicalDeviceSurfacePresentModesKHR(
      r->physical_device, r->surface, &mode_count, modes);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  for (i = 0; i < (owl_i32)mode_count; ++i) {
    r->swapchain_present_mode = modes[mode_count - i - 1];

    if (preferred == r->swapchain_present_mode) {
      break;
    }
  }

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_swapchain_init(struct owl_renderer_init_info const *info,
                            struct owl_renderer *r) {
  owl_u32 families[2];
  VkSurfaceCapabilitiesKHR capabilities;
  VkSwapchainCreateInfoKHR swapchain_info;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  families[0] = r->graphics_queue_family;
  families[1] = r->present_queue_family;

  r->swapchain_extent.width = (owl_u32)info->framebuffer_width;
  r->swapchain_extent.height = (owl_u32)info->framebuffer_height;
  owl_renderer_swapchain_extent_clamp(r);

#if 1
  r->swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
#else
  r->swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
#endif

  if (OWL_SUCCESS != (code = owl_renderer_present_mode_ensure(r))) {
    goto out;
  }

  vkres = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r->physical_device,
                                                    r->surface, &capabilities);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = NULL;
  swapchain_info.flags = 0;
  swapchain_info.surface = r->surface;
  swapchain_info.minImageCount = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
  swapchain_info.imageFormat = r->surface_format.format;
  swapchain_info.imageColorSpace = r->surface_format.colorSpace;
  swapchain_info.imageExtent.width = r->swapchain_extent.width;
  swapchain_info.imageExtent.height = r->swapchain_extent.height;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_info.preTransform = capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = r->swapchain_present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  if (r->graphics_queue_family == r->present_queue_family) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = NULL;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = families;
  }

  vkres = vkCreateSwapchainKHR(r->device, &swapchain_info, NULL, &r->swapchain);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  vkres = vkGetSwapchainImagesKHR(r->device, r->swapchain,
                                  &r->swapchain_image_count, NULL);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out_err_swapchain_deinit;
  }

  if (OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT <= r->swapchain_image_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto out_err_swapchain_deinit;
  }

  vkres = vkGetSwapchainImagesKHR(
      r->device, r->swapchain, &r->swapchain_image_count, r->swapchain_images);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out_err_swapchain_deinit;
  }

  r->swapchain_clear_values[0].color.float32[0] = 0.01F;
  r->swapchain_clear_values[0].color.float32[1] = 0.01F;
  r->swapchain_clear_values[0].color.float32[2] = 0.01F;
  r->swapchain_clear_values[0].color.float32[3] = 1.0F;
  r->swapchain_clear_values[1].depthStencil.depth = 1.0F;
  r->swapchain_clear_values[1].depthStencil.stencil = 0.0F;

  goto out;

out_err_swapchain_deinit:
  vkDestroySwapchainKHR(r->device, r->swapchain, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_swapchain_deinit(struct owl_renderer *r) {
  vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_swapchain_image_views_init(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)r->swapchain_image_count; ++i) {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = r->swapchain_images[i];
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = r->surface_format.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vkres =
        vkCreateImageView(r->device, &info, NULL, &r->swapchain_image_views[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_image_views_deinit;
    }
  }

  goto out;

out_err_image_views_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyImageView(r->device, r->swapchain_image_views[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void
owl_renderer_swapchain_image_views_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < (owl_i32)r->swapchain_image_count; ++i) {
    vkDestroyImageView(r->device, r->swapchain_image_views[i], NULL);
  }
}

OWL_INTERNAL enum owl_code owl_renderer_pools_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkCommandPoolCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = r->graphics_queue_family;

    vkres =
        vkCreateCommandPool(r->device, &info, NULL, &r->transient_command_pool);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

  {
    /* NOTE: 256 ought to be enough... */
    VkDescriptorPoolSize sizes[6];
    VkDescriptorPoolCreateInfo info;

    sizes[0].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    sizes[1].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

    sizes[2].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    sizes[3].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    sizes[4].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    sizes[5].descriptorCount = OWL_JUST_ENOUGH_DESCRIPTORS;
    sizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = OWL_JUST_ENOUGH_DESCRIPTORS * OWL_ARRAY_SIZE(sizes);
    info.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    info.pPoolSizes = sizes;

    vkres = vkCreateDescriptorPool(r->device, &info, NULL, &r->common_set_pool);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_transient_command_pool_deinit;
    }
  }

  goto out;

out_transient_command_pool_deinit:
  vkDestroyCommandPool(r->device, r->transient_command_pool, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_pools_deinit(struct owl_renderer *r) {
  vkDestroyDescriptorPool(r->device, r->common_set_pool, NULL);
  vkDestroyCommandPool(r->device, r->transient_command_pool, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_depth_stencil_format_select(struct owl_renderer *r) {
  VkFormatProperties properties;

  enum owl_code code = OWL_SUCCESS;

  vkGetPhysicalDeviceFormatProperties(r->physical_device,
                                      VK_FORMAT_D24_UNORM_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    r->depth_stencil_format = VK_FORMAT_D24_UNORM_S8_UINT;
    goto out;
  }

  vkGetPhysicalDeviceFormatProperties(
      r->physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    r->depth_stencil_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    goto out;
  }

  code = OWL_ERROR_UNKNOWN;

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_main_render_pass_init(struct owl_renderer *r) {
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo info;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_depth_stencil_format_select(r))) {
    goto out;
  }

  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  resolve_reference.attachment = 2;
  resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  attachments[0].flags = 0;
  attachments[0].format = r->surface_format.format;
  attachments[0].samples = r->msaa_sample_count;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  attachments[1].flags = 0;
  attachments[1].format = r->depth_stencil_format;
  attachments[1].samples = r->msaa_sample_count;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  attachments[2].flags = 0;
  attachments[2].format = r->surface_format.format;
  attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_reference;
  subpass.pResolveAttachments = &resolve_reference;
  subpass.pDepthStencilAttachment = &depth_reference;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = 0;

  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.attachmentCount = OWL_ARRAY_SIZE(attachments);
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  vkres = vkCreateRenderPass(r->device, &info, NULL, &r->main_render_pass);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_main_render_pass_deinit(struct owl_renderer *r) {
  vkDestroyRenderPass(r->device, r->main_render_pass, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_attachments_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkImageCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = r->surface_format.format;
    info.extent.width = r->swapchain_extent.width;
    info.extent.height = r->swapchain_extent.height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = r->msaa_sample_count;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkres = vkCreateImage(r->device, &info, NULL, &r->color_image);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetImageMemoryRequirements(r->device, r->color_image, &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    vkres = vkAllocateMemory(r->device, &info, NULL, &r->color_memory);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_color_image_deinit;
    }

    vkres = vkBindImageMemory(r->device, r->color_image, r->color_memory, 0);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_color_memory_deinit;
    }
  }

  {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = r->color_image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = r->surface_format.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vkres = vkCreateImageView(r->device, &info, NULL, &r->color_image_view);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_color_memory_deinit;
    }
  }

  {
    VkImageCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = r->depth_stencil_format;
    info.extent.width = r->swapchain_extent.width;
    info.extent.height = r->swapchain_extent.height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = r->msaa_sample_count;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkres = vkCreateImage(r->device, &info, NULL, &r->depth_stencil_image);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_color_image_view_deinit;
    }
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetImageMemoryRequirements(r->device, r->depth_stencil_image,
                                 &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    vkres = vkAllocateMemory(r->device, &info, NULL, &r->depth_stencil_memory);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_depth_stencil_image_deinit;
    }

    vkres = vkBindImageMemory(r->device, r->depth_stencil_image,
                              r->depth_stencil_memory, 0);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_depth_stencil_memory_deinit;
    }
  }

  {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = r->depth_stencil_image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = r->depth_stencil_format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vkres =
        vkCreateImageView(r->device, &info, NULL, &r->depth_stencil_image_view);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_depth_stencil_memory_deinit;
    }
  }

  goto out;

out_err_depth_stencil_memory_deinit:
  vkFreeMemory(r->device, r->depth_stencil_memory, NULL);

out_err_depth_stencil_image_deinit:
  vkDestroyImage(r->device, r->depth_stencil_image, NULL);

out_err_color_image_view_deinit:
  vkDestroyImageView(r->device, r->color_image_view, NULL);

out_err_color_memory_deinit:
  vkFreeMemory(r->device, r->color_memory, NULL);

out_err_color_image_deinit:
  vkDestroyImage(r->device, r->color_image, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_attachments_deinit(struct owl_renderer *r) {
  vkDestroyImageView(r->device, r->depth_stencil_image_view, NULL);
  vkFreeMemory(r->device, r->depth_stencil_memory, NULL);
  vkDestroyImage(r->device, r->depth_stencil_image, NULL);

  vkDestroyImageView(r->device, r->color_image_view, NULL);
  vkFreeMemory(r->device, r->color_memory, NULL);
  vkDestroyImage(r->device, r->color_image, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_swapchain_framebuffers_init(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)r->swapchain_image_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo info;

    attachments[0] = r->color_image_view;
    attachments[1] = r->depth_stencil_image_view;
    attachments[2] = r->swapchain_image_views[i];

    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.renderPass = r->main_render_pass;
    info.attachmentCount = OWL_ARRAY_SIZE(attachments);
    info.pAttachments = attachments;
    info.width = r->swapchain_extent.width;
    info.height = r->swapchain_extent.height;
    info.layers = 1;

    vkres = vkCreateFramebuffer(r->device, &info, NULL,
                                &r->swapchain_framebuffers[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_swapchain_framebuffers_deinit;
    }
  }

  goto out;

out_err_swapchain_framebuffers_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyFramebuffer(r->device, r->swapchain_framebuffers[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void
owl_renderer_swapchain_framebuffers_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < (owl_i32)r->swapchain_image_count; ++i) {
    vkDestroyFramebuffer(r->device, r->swapchain_framebuffers[i], NULL);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_set_layouts_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vkres = vkCreateDescriptorSetLayout(r->device, &info, NULL,
                                        &r->vertex_ubo_set_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      code = OWL_SUCCESS;
      goto out;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vkres = vkCreateDescriptorSetLayout(r->device, &info, NULL,
                                        &r->fragment_ubo_set_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_vertex_ubo_set_layout_deinit;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vkres = vkCreateDescriptorSetLayout(r->device, &info, NULL,
                                        &r->shared_ubo_set_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_fragment_ubo_set_layout_denit;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vkres = vkCreateDescriptorSetLayout(r->device, &info, NULL,
                                        &r->vertex_ssbo_set_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_shared_ubo_set_layout_deinit;
    }
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo info;

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = OWL_ARRAY_SIZE(bindings);
    info.pBindings = bindings;

    vkres = vkCreateDescriptorSetLayout(r->device, &info, NULL,
                                        &r->image_set_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_vertex_ssbo_set_layout_denit;
    }
  }

  goto out;

out_err_vertex_ssbo_set_layout_denit:
  vkDestroyDescriptorSetLayout(r->device, r->vertex_ssbo_set_layout, NULL);

out_err_shared_ubo_set_layout_deinit:
  vkDestroyDescriptorSetLayout(r->device, r->shared_ubo_set_layout, NULL);

out_err_fragment_ubo_set_layout_denit:
  vkDestroyDescriptorSetLayout(r->device, r->fragment_ubo_set_layout, NULL);

out_err_vertex_ubo_set_layout_deinit:
  vkDestroyDescriptorSetLayout(r->device, r->vertex_ubo_set_layout, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_set_layouts_deinit(struct owl_renderer *r) {
  vkDestroyDescriptorSetLayout(r->device, r->image_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->vertex_ssbo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->shared_ubo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->fragment_ubo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->vertex_ubo_set_layout, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_pipeline_layouts_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  r->active_pipeline = VK_NULL_HANDLE;
  r->active_pipeline_layout = VK_NULL_HANDLE;

  {
    VkDescriptorSetLayout sets[2];
    VkPipelineLayoutCreateInfo info;

    sets[0] = r->vertex_ubo_set_layout;
    sets[1] = r->image_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = OWL_ARRAY_SIZE(sets);
    info.pSetLayouts = sets;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = NULL;

    vkres = vkCreatePipelineLayout(r->device, &info, NULL,
                                   &r->common_pipeline_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

  {
    VkPushConstantRange push_constant;
    VkDescriptorSetLayout sets[6];
    VkPipelineLayoutCreateInfo info;

    push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(struct owl_model_material_push_constant);

    sets[0] = r->shared_ubo_set_layout;
    sets[1] = r->image_set_layout;
    sets[2] = r->image_set_layout;
    sets[3] = r->image_set_layout;
    sets[4] = r->vertex_ssbo_set_layout;
    sets[5] = r->fragment_ubo_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = OWL_ARRAY_SIZE(sets);
    info.pSetLayouts = sets;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push_constant;

    vkres = vkCreatePipelineLayout(r->device, &info, NULL,
                                   &r->model_pipeline_layout);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_common_pipeline_layout_deinit;
    }
  }

  goto out;

out_err_common_pipeline_layout_deinit:
  vkDestroyPipelineLayout(r->device, r->common_pipeline_layout, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_pipeline_layouts_deinit(struct owl_renderer *r) {
  vkDestroyPipelineLayout(r->device, r->model_pipeline_layout, NULL);
  vkDestroyPipelineLayout(r->device, r->common_pipeline_layout, NULL);
}

OWL_INTERNAL enum owl_code owl_renderer_shaders_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo info;

    /* TODO(samuel): Properly load code at runtime */
    OWL_LOCAL_PERSIST owl_u32 const spv[] = {
#include "owl_glsl_basic.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vkres =
        vkCreateShaderModule(r->device, &info, NULL, &r->basic_vertex_shader);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    OWL_LOCAL_PERSIST owl_u32 const spv[] = {
#include "owl_glsl_basic.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vkres =
        vkCreateShaderModule(r->device, &info, NULL, &r->basic_fragment_shader);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_basic_vertex_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    OWL_LOCAL_PERSIST owl_u32 const spv[] = {
#include "owl_glsl_font.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vkres =
        vkCreateShaderModule(r->device, &info, NULL, &r->font_fragment_shader);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_basic_fragment_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    OWL_LOCAL_PERSIST owl_u32 const spv[] = {
#include "owl_glsl_model.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vkres =
        vkCreateShaderModule(r->device, &info, NULL, &r->model_vertex_shader);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_font_fragment_shader_deinit;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    OWL_LOCAL_PERSIST owl_u32 const spv[] = {
#include "owl_glsl_model.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vkres =
        vkCreateShaderModule(r->device, &info, NULL, &r->model_fragment_shader);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_model_vertex_shader_deinit;
    }
  }

  goto out;

out_err_model_vertex_shader_deinit:
  vkDestroyShaderModule(r->device, r->model_vertex_shader, NULL);

out_err_font_fragment_shader_deinit:
  vkDestroyShaderModule(r->device, r->font_fragment_shader, NULL);

out_err_basic_fragment_shader_deinit:
  vkDestroyShaderModule(r->device, r->basic_fragment_shader, NULL);

out_err_basic_vertex_shader_deinit:
  vkDestroyShaderModule(r->device, r->basic_vertex_shader, NULL);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_shaders_deinit(struct owl_renderer *r) {
  vkDestroyShaderModule(r->device, r->model_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->model_vertex_shader, NULL);
  vkDestroyShaderModule(r->device, r->font_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->basic_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->basic_vertex_shader, NULL);
}

OWL_INTERNAL enum owl_code owl_renderer_pipelines_init(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_PIPELINE_COUNT; ++i) {
    VkVertexInputBindingDescription vertex_bindings[8];
    VkVertexInputAttributeDescription vertex_attrs[8];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo resterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_blend_attachments[8];
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo info;

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
      vertex_bindings[0].binding = 0;
      vertex_bindings[0].stride = sizeof(struct owl_draw_command_vertex);
      vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attrs[0].binding = 0;
      vertex_attrs[0].location = 0;
      vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[0].offset =
          offsetof(struct owl_draw_command_vertex, position);

      vertex_attrs[1].binding = 0;
      vertex_attrs[1].location = 1;
      vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[1].offset = offsetof(struct owl_draw_command_vertex, color);

      vertex_attrs[2].binding = 0;
      vertex_attrs[2].location = 2;
      vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[2].offset = offsetof(struct owl_draw_command_vertex, uv);
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      vertex_bindings[0].binding = 0;
      vertex_bindings[0].stride = sizeof(struct owl_model_vertex);
      vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attrs[0].binding = 0;
      vertex_attrs[0].location = 0;
      vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[0].offset = offsetof(struct owl_model_vertex, position);

      vertex_attrs[1].binding = 0;
      vertex_attrs[1].location = 1;
      vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[1].offset = offsetof(struct owl_model_vertex, normal);

      vertex_attrs[2].binding = 0;
      vertex_attrs[2].location = 2;
      vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[2].offset = offsetof(struct owl_model_vertex, uv0);

      vertex_attrs[3].binding = 0;
      vertex_attrs[3].location = 3;
      vertex_attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[3].offset = offsetof(struct owl_model_vertex, uv1);

      vertex_attrs[4].binding = 0;
      vertex_attrs[4].location = 4;
      vertex_attrs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attrs[4].offset = offsetof(struct owl_model_vertex, joints0);

      vertex_attrs[5].binding = 0;
      vertex_attrs[5].location = 5;
      vertex_attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attrs[5].offset = offsetof(struct owl_model_vertex, weights0);
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
      vertex_input.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertex_input.pNext = NULL;
      vertex_input.flags = 0;
      vertex_input.vertexBindingDescriptionCount = 1;
      vertex_input.pVertexBindingDescriptions = vertex_bindings;
      vertex_input.vertexAttributeDescriptionCount = 3;
      vertex_input.pVertexAttributeDescriptions = vertex_attrs;
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      vertex_input.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertex_input.pNext = NULL;
      vertex_input.flags = 0;
      vertex_input.vertexBindingDescriptionCount = 1;
      vertex_input.pVertexBindingDescriptions = vertex_bindings;
      vertex_input.vertexAttributeDescriptionCount = 6;
      vertex_input.pVertexAttributeDescriptions = vertex_attrs;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      input_assembly.sType =
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      input_assembly.pNext = NULL;
      input_assembly.flags = 0;
      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      input_assembly.primitiveRestartEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      viewport.x = 0.0F;
      viewport.y = 0.0F;
      viewport.width = r->swapchain_extent.width;
      viewport.height = r->swapchain_extent.height;
      viewport.minDepth = 0.0F;
      viewport.maxDepth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent = r->swapchain_extent;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      viewport_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      viewport_state.pNext = NULL;
      viewport_state.flags = 0;
      viewport_state.viewportCount = 1;
      viewport_state.pViewports = &viewport;
      viewport_state.scissorCount = 1;
      viewport_state.pScissors = &scissor;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_FONT:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_FILL;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_FILL;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;

    case OWL_RENDERER_PIPELINE_WIRES:
      resterization.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      resterization.pNext = NULL;
      resterization.flags = 0;
      resterization.depthClampEnable = VK_FALSE;
      resterization.rasterizerDiscardEnable = VK_FALSE;
      resterization.polygonMode = VK_POLYGON_MODE_LINE;
      resterization.cullMode = VK_CULL_MODE_BACK_BIT;
      resterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      resterization.depthBiasEnable = VK_FALSE;
      resterization.depthBiasConstantFactor = 0.0F;
      resterization.depthBiasClamp = 0.0F;
      resterization.depthBiasSlopeFactor = 0.0F;
      resterization.lineWidth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      multisample.sType =
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisample.pNext = NULL;
      multisample.flags = 0;
      multisample.rasterizationSamples = r->msaa_sample_count;
      multisample.sampleShadingEnable = VK_FALSE;
      multisample.minSampleShading = 1.0F;
      multisample.pSampleMask = NULL;
      multisample.alphaToCoverageEnable = VK_FALSE;
      multisample.alphaToOneEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_MODEL:
      color_blend_attachments[0].blendEnable = VK_FALSE;
      color_blend_attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
      color_blend_attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
      color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      color_blend_attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;

    case OWL_RENDERER_PIPELINE_FONT:
      color_blend_attachments[0].blendEnable = VK_TRUE;
      color_blend_attachments[0].srcColorBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachments[0].dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].srcAlphaBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachments[0].dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachments[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      color_blend.sType =
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      color_blend.pNext = NULL;
      color_blend.flags = 0;
      color_blend.logicOpEnable = VK_FALSE;
      color_blend.logicOp = VK_LOGIC_OP_COPY;
      color_blend.attachmentCount = 1;
      color_blend.pAttachments = color_blend_attachments;
      color_blend.blendConstants[0] = 0.0F;
      color_blend.blendConstants[1] = 0.0F;
      color_blend.blendConstants[2] = 0.0F;
      color_blend.blendConstants[3] = 0.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      depth_stencil.sType =
          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depth_stencil.pNext = NULL;
      depth_stencil.flags = 0;
      depth_stencil.depthTestEnable = VK_TRUE;
      depth_stencil.depthWriteEnable = VK_TRUE;
      depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
      depth_stencil.depthBoundsTestEnable = VK_FALSE;
      depth_stencil.stencilTestEnable = VK_FALSE;
      OWL_MEMSET(&depth_stencil.front, 0, sizeof(depth_stencil.front));
      OWL_MEMSET(&depth_stencil.back, 0, sizeof(depth_stencil.back));
      depth_stencil.minDepthBounds = 0.0F;
      depth_stencil.maxDepthBounds = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = r->basic_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = r->basic_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_RENDERER_PIPELINE_FONT:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = r->basic_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = r->font_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = r->model_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = r->model_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
      r->pipeline_layouts[i] = r->common_pipeline_layout;
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      r->pipeline_layouts[i] = r->model_pipeline_layout;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_MAIN:
    case OWL_RENDERER_PIPELINE_WIRES:
    case OWL_RENDERER_PIPELINE_FONT:
    case OWL_RENDERER_PIPELINE_MODEL:
      info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.stageCount = OWL_ARRAY_SIZE(stages);
      info.pStages = stages;
      info.pVertexInputState = &vertex_input;
      info.pInputAssemblyState = &input_assembly;
      info.pTessellationState = NULL;
      info.pViewportState = &viewport_state;
      info.pRasterizationState = &resterization;
      info.pMultisampleState = &multisample;
      info.pDepthStencilState = &depth_stencil;
      info.pColorBlendState = &color_blend;
      info.pDynamicState = NULL;
      info.layout = r->pipeline_layouts[i];
      info.renderPass = r->main_render_pass;
      info.subpass = 0;
      info.basePipelineHandle = VK_NULL_HANDLE;
      info.basePipelineIndex = -1;
      break;
    }

    vkres = vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &info, NULL,
                                      &r->pipelines[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_pipelines_deinit;
    }
  }

  goto out;

out_err_pipelines_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyPipeline(r->device, r->pipelines[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_pipelines_deinit(struct owl_renderer *r) {
  owl_i32 i;
  for (i = OWL_RENDERER_PIPELINE_COUNT - 1; i >= 0; --i) {
    vkDestroyPipeline(r->device, r->pipelines[i], NULL);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_frame_commands_init(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  r->active_frame = 0;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkCommandPoolCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.queueFamilyIndex = r->graphics_queue_family;

    vkres =
        vkCreateCommandPool(r->device, &info, NULL, &r->frame_command_pools[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_command_pools_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkCommandBufferAllocateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = r->frame_command_pools[i];
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vkres = vkAllocateCommandBuffers(r->device, &info,
                                     &r->frame_command_buffers[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_command_buffers_deinit;
    }
  }

  r->active_frame_command_buffer = r->frame_command_buffers[r->active_frame];
  r->active_frame_command_pool = r->frame_command_pools[r->active_frame];

  goto out;

out_err_frame_command_buffers_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkFreeCommandBuffers(r->device, r->frame_command_pools[i], 1,
                         &r->frame_command_buffers[i]);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_err_frame_command_pools_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyCommandPool(r->device, r->frame_command_pools[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_frame_commands_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkFreeCommandBuffers(r->device, r->frame_command_pools[i], 1,
                         &r->frame_command_buffers[i]);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyCommandPool(r->device, r->frame_command_pools[i], NULL);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_frame_sync_init(struct owl_renderer *r) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkFenceCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkres = vkCreateFence(r->device, &info, NULL, &r->in_flight_fences[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_in_flight_fences_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkSemaphoreCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vkres = vkCreateSemaphore(r->device, &info, NULL,
                              &r->render_done_semaphores[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_render_done_semaphores_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkSemaphoreCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vkres = vkCreateSemaphore(r->device, &info, NULL,
                              &r->image_available_semaphores[i]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_image_available_semaphores_deinit;
    }
  }

  r->active_in_flight_fence = r->in_flight_fences[r->active_frame];

  r->active_render_done_semaphore = r->render_done_semaphores[r->active_frame];

  r->active_image_available_semaphore =
      r->image_available_semaphores[r->active_frame];

  goto out;

out_err_image_available_semaphores_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroySemaphore(r->device, r->image_available_semaphores[i], NULL);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_err_render_done_semaphores_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroySemaphore(r->device, r->render_done_semaphores[i], NULL);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_err_in_flight_fences_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyFence(r->device, r->in_flight_fences[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_frame_sync_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyFence(r->device, r->in_flight_fences[i], NULL);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroySemaphore(r->device, r->render_done_semaphores[i], NULL);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroySemaphore(r->device, r->image_available_semaphores[i], NULL);
  }
}

OWL_INTERNAL enum owl_code owl_renderer_garbage_init(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  r->garbage_buffer_count = 0;
  r->garbage_memory_count = 0;
  r->garbage_common_set_count = 0;
  r->garbage_model2_set_count = 0;
  r->garbage_model1_set_count = 0;

  return code;
}

OWL_INTERNAL void
owl_renderer_frame_heap_garbage_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < r->garbage_model1_set_count; ++i) {
    vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                         &r->garbage_model2_sets[i]);
  }

  r->garbage_model1_set_count = 0;

  for (i = 0; i < r->garbage_model2_set_count; ++i) {
    vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                         &r->garbage_model1_sets[i]);
  }

  r->garbage_model2_set_count = 0;

  for (i = 0; i < r->garbage_common_set_count; ++i) {
    vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                         &r->garbage_common_sets[i]);
  }

  r->garbage_common_set_count = 0;

  for (i = 0; i < r->garbage_memory_count; ++i) {
    vkFreeMemory(r->device, r->garbage_memories[i], NULL);
  }

  r->garbage_memory_count = 0;

  for (i = 0; i < r->garbage_buffer_count; ++i) {
    vkDestroyBuffer(r->device, r->garbage_buffers[i], NULL);
  }

  r->garbage_buffer_count = 0;
}

OWL_INTERNAL enum owl_code owl_renderer_frame_heap_init(struct owl_renderer *r,
                                                        owl_u64 sz) {
  owl_i32 i;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  r->frame_heap_buffer_size = sz;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    r->frame_heap_offsets[i] = 0;
  }

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = sz;
    info.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      vkres = vkCreateBuffer(r->device, &info, NULL, &r->frame_heap_buffers[i]);

      if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
        goto out_err_frame_heap_buffers_deinit;
      }
    }
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements(r->device, r->frame_heap_buffers[0],
                                  &requirements);

    r->frame_heap_buffer_alignment = requirements.alignment;
    r->frame_heap_buffer_aligned_size = OWL_ALIGNU2(sz, requirements.alignment);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize =
        r->frame_heap_buffer_aligned_size * OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

    vkres = vkAllocateMemory(r->device, &info, NULL, &r->frame_heap_memory);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_heap_buffers_deinit;
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      vkres = vkBindBufferMemory(r->device, r->frame_heap_buffers[i],
                                 r->frame_heap_memory,
                                 i * r->frame_heap_buffer_aligned_size);

      if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
        goto out_err_frame_heap_memory_deinit;
      }
    }
  }

  {
    void *data;

    vkres = vkMapMemory(r->device, r->frame_heap_memory, 0, VK_WHOLE_SIZE, 0,
                        &data);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_heap_memory_deinit;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      r->frame_heap_data[i] =
          &((owl_byte *)data)[i * r->frame_heap_buffer_aligned_size];
    }
  }

  {
    VkDescriptorSetAllocateInfo info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      layouts[i] = r->vertex_ubo_set_layout;
    }

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->common_set_pool;
    info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
    info.pSetLayouts = layouts;

    vkres =
        vkAllocateDescriptorSets(r->device, &info, r->frame_heap_common_sets);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_heap_memory_unmap;
    }
  }

  {
    VkDescriptorSetAllocateInfo info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      layouts[i] = r->shared_ubo_set_layout;
    }

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->common_set_pool;
    info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
    info.pSetLayouts = layouts;

    vkres =
        vkAllocateDescriptorSets(r->device, &info, r->frame_heap_model1_sets);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_heap_common_ubo_sets_deinit;
    }
  }

  {
    VkDescriptorSetAllocateInfo info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAME_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      layouts[i] = r->fragment_ubo_set_layout;
    }

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->common_set_pool;
    info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
    info.pSetLayouts = layouts;

    vkres =
        vkAllocateDescriptorSets(r->device, &info, r->frame_heap_model2_sets);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_frame_heap_model_ubo_sets_deinit;
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buffer;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_draw_command_uniform);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = r->frame_heap_common_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_model_uniform);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = r->frame_heap_model1_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_model_uniform_params);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = r->frame_heap_model2_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(r->device, 1, &write, 0, NULL);
    }
  }

  r->active_frame_heap_data = r->frame_heap_data[r->active_frame];

  r->active_frame_heap_buffer = r->frame_heap_buffers[r->active_frame];

  r->active_frame_heap_common_set = r->frame_heap_common_sets[r->active_frame];

  r->active_frame_heap_model1_set = r->frame_heap_model1_sets[r->active_frame];

  r->active_frame_heap_model2_set = r->frame_heap_model2_sets[r->active_frame];

  goto out;

out_err_frame_heap_model_ubo_sets_deinit:
  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                       r->frame_heap_common_sets);

out_err_frame_heap_common_ubo_sets_deinit:
  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                       r->frame_heap_common_sets);

out_err_frame_heap_memory_unmap:
  vkUnmapMemory(r->device, r->frame_heap_memory);

out_err_frame_heap_memory_deinit:
  vkFreeMemory(r->device, r->frame_heap_memory, NULL);

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_err_frame_heap_buffers_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyBuffer(r->device, r->frame_heap_buffers[i], NULL);
  }

out:
  return code;
}

OWL_INTERNAL void owl_renderer_frame_heap_deinit(struct owl_renderer *r) {
  owl_i32 i;

  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                       r->frame_heap_model2_sets);

  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                       r->frame_heap_model1_sets);

  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                       r->frame_heap_common_sets);

  vkFreeMemory(r->device, r->frame_heap_memory, NULL);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyBuffer(r->device, r->frame_heap_buffers[i], NULL);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_frame_heap_move_to_garbage(struct owl_renderer *r) {
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  {
    owl_i32 prev = r->garbage_buffer_count;
    owl_i32 next = prev + OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT <= next) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      r->garbage_buffers[prev + i] = r->frame_heap_buffers[i];
    }

    r->garbage_buffer_count = next;
  }

  {
    owl_i32 prev = r->garbage_common_set_count;
    owl_i32 next = prev + OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT <= next) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      r->garbage_common_sets[prev + i] = r->frame_heap_common_sets[i];
    }

    r->garbage_common_set_count = next;
  }

  {
    owl_i32 prev = r->garbage_model2_set_count;
    owl_i32 next = prev + OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT <= next) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      r->garbage_model1_sets[prev + i] = r->frame_heap_model1_sets[i];
    }

    r->garbage_model2_set_count = next;
  }

  {
    owl_i32 prev = r->garbage_model1_set_count;
    owl_i32 next = prev + OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT <= next) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      r->garbage_model2_sets[prev + i] = r->frame_heap_model2_sets[i];
    }

    r->garbage_model2_set_count = next;
  }

  {
    owl_i32 prev = r->garbage_memory_count;
    owl_i32 next = prev + 1;

    if (OWL_RENDERER_MAX_GARBAGE_ITEM_COUNT <= next) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto out;
    }

    r->garbage_memories[prev + 0] = r->frame_heap_memory;

    r->garbage_memory_count = next;
  }

  r->active_frame_heap_data = NULL;
  r->active_frame_heap_buffer = VK_NULL_HANDLE;
  r->active_frame_heap_common_set = VK_NULL_HANDLE;
  r->active_frame_heap_model1_set = VK_NULL_HANDLE;
  r->active_frame_heap_model2_set = VK_NULL_HANDLE;

out:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_image_pool_init(struct owl_renderer *r) {
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    r->image_pool_slots[i] = 0;
  }

  return code;
}

OWL_INTERNAL void owl_renderer_image_pool_deinit(struct owl_renderer *r) {
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    if (!r->image_pool_slots[i]) {
      continue;
    }

    r->image_pool_slots[i] = 0;

    vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                         &r->image_pool_sets[i]);

    vkDestroySampler(r->device, r->image_pool_samplers[i], NULL);

    vkDestroyImageView(r->device, r->image_pool_image_views[i], NULL);

    vkFreeMemory(r->device, r->image_pool_memories[i], NULL);

    vkDestroyImage(r->device, r->image_pool_images[i], NULL);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_frame_heap_reserve(struct owl_renderer *r, owl_u64 sz) {
  enum owl_code code = OWL_SUCCESS;

  owl_u64 required = r->frame_heap_offsets[r->active_frame] + sz;

  /* if the current buffer size is enough do nothing */
  if (required < r->frame_heap_buffer_size) {
    goto out;
  }

  /* unmap the memory as it's still going to be kept alive */
  vkUnmapMemory(r->device, r->frame_heap_memory);

  /* move the current frame heap resources to the garbage */
  code = owl_renderer_frame_heap_move_to_garbage(r);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  /* create a new frame heap with double the required size*/
  code = owl_renderer_frame_heap_init(r, 2 * required);

  if (OWL_SUCCESS != code) {
    goto out;
  }

out:
  return code;
}

enum owl_code
owl_renderer_swapchain_resize(struct owl_renderer_init_info const *info,
                              struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  /* set the current window and framebuffer dims */
  r->window_width = info->window_width;
  r->window_height = info->window_height;
  r->framebuffer_width = info->framebuffer_width;
  r->framebuffer_height = info->framebuffer_height;
  r->framebuffer_ratio = info->framebuffer_ratio;

  /* make sure no resources are in use */
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  /* deinit all resources that depend on ethier the swapchain dims or the
   * swapchain images */
  owl_renderer_pipelines_deinit(r);
  owl_renderer_swapchain_framebuffers_deinit(r);
  owl_renderer_attachments_deinit(r);
  owl_renderer_main_render_pass_deinit(r);
  owl_renderer_frame_sync_deinit(r);
  owl_renderer_swapchain_image_views_deinit(r);
  owl_renderer_swapchain_deinit(r);

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_init(info, r))) {
    goto out;
  }

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_image_views_init(r))) {
    goto out_err_swapchain_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_frame_sync_init(r))) {
    goto out_err_swapchain_views;
  }

  if (OWL_SUCCESS != (code = owl_renderer_main_render_pass_init(r))) {
    goto out_err_frame_sync_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_attachments_init(r))) {
    goto out_err_main_render_pass_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_framebuffers_init(r))) {
    goto out_err_attachments_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_pipelines_init(r))) {
    goto out_err_swapchain_framebuffers_deinit;
  }

  goto out;

out_err_swapchain_framebuffers_deinit:
  owl_renderer_swapchain_framebuffers_deinit(r);

out_err_attachments_deinit:
  owl_renderer_attachments_deinit(r);

out_err_main_render_pass_deinit:
  owl_renderer_main_render_pass_deinit(r);

out_err_frame_sync_deinit:
  owl_renderer_frame_sync_deinit(r);

out_err_swapchain_views:
  owl_renderer_swapchain_image_views_deinit(r);

out_err_swapchain_deinit:
  owl_renderer_swapchain_deinit(r);

out:
  return code;
}

owl_b32 owl_renderer_frame_heap_offset_is_clear(struct owl_renderer const *r) {
  return 0 == r->frame_heap_offsets[r->active_frame];
}

OWL_INTERNAL void
owl_renderer_frame_heap_garbage_clear(struct owl_renderer *r) {
  owl_renderer_frame_heap_garbage_deinit(r);
}

void owl_renderer_frame_heap_offset_clear(struct owl_renderer *r) {
  r->frame_heap_offsets[r->active_frame] = 0;
}

void *
owl_renderer_frame_heap_alloc(struct owl_renderer *r, owl_u64 sz,
                              struct owl_renderer_frame_heap_reference *ref) {
  owl_u64 offset;

  owl_byte *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  /* make sure the current heap size is enought */
  if (OWL_SUCCESS != (code = owl_renderer_frame_heap_reserve(r, sz))) {
    goto out;
  }

  offset = r->frame_heap_offsets[r->active_frame];

  ref->offset32 = (owl_u32)offset;
  ref->offset = offset;
  ref->buffer = r->active_frame_heap_buffer;
  ref->common_ubo_set = r->active_frame_heap_common_set;
  ref->model_ubo_set = r->active_frame_heap_model1_set;
  ref->model_ubo_params_set = r->active_frame_heap_model2_set;

  data = &r->active_frame_heap_data[offset];

  offset = OWL_ALIGNU2(offset + sz, r->frame_heap_buffer_alignment);
  r->frame_heap_offsets[r->active_frame] = offset;

out:
  return data;
}

enum owl_code
owl_renderer_frame_heap_submit(struct owl_renderer *r, owl_u64 sz,
                               void const *src,
                               struct owl_renderer_frame_heap_reference *ref) {
  owl_byte *data;

  enum owl_code code = OWL_SUCCESS;

  if (!(data = owl_renderer_frame_heap_alloc(r, sz, ref))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out;
  }

  OWL_MEMCPY(data, src, sz);

out:
  return code;
}

owl_u32 owl_renderer_find_memory_type(struct owl_renderer const *r,
                                      VkMemoryRequirements const *req,
                                      enum owl_renderer_memory_visibility vis) {
  owl_i32 i;
  VkMemoryPropertyFlags visibility;
  VkPhysicalDeviceMemoryProperties properties;

  owl_u32 const filter = req->memoryTypeBits;

  switch (vis) {
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU:
    visibility = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU_CACHED:
    visibility = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT:
    visibility = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_GPU:
    visibility = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  }

  vkGetPhysicalDeviceMemoryProperties(r->physical_device, &properties);

  for (i = 0; i < (owl_i32)properties.memoryTypeCount; ++i) {
    owl_u32 flags = properties.memoryTypes[i].propertyFlags;

    if ((flags & visibility) && (filter & (1U << i))) {
      return i;
    }
  }

  return OWL_RENDERER_MEMORY_TYPE_NONE;
}

enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *r,
                                         enum owl_renderer_pipeline pipeline) {
  enum owl_code code = OWL_SUCCESS;

  if (OWL_RENDERER_PIPELINE_NONE == pipeline) {
    goto out;
  }

  r->active_pipeline = r->pipelines[pipeline];
  r->active_pipeline_layout = r->pipeline_layouts[pipeline];

  vkCmdBindPipeline(r->active_frame_command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS, r->active_pipeline);

out:
  return code;
}

enum owl_code
owl_renderer_immidiate_command_buffer_init(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(!r->immidiate_command_buffer);

  {
    VkCommandBufferAllocateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = r->transient_command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vkres = vkAllocateCommandBuffers(r->device, &info,
                                     &r->immidiate_command_buffer);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

out:
  return code;
}

enum owl_code
owl_renderer_immidiate_command_buffer_begin(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(r->immidiate_command_buffer);

  {
    VkCommandBufferBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    vkres = vkBeginCommandBuffer(r->immidiate_command_buffer, &info);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out;
    }
  }

out:
  return code;
}

enum owl_code
owl_renderer_immidiate_command_buffer_end(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(r->immidiate_command_buffer);

  vkres = vkEndCommandBuffer(r->immidiate_command_buffer);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

enum owl_code
owl_renderer_immidiate_command_buffer_submit(struct owl_renderer *r) {
  VkSubmitInfo info;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(r->immidiate_command_buffer);

  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = NULL;
  info.waitSemaphoreCount = 0;
  info.pWaitSemaphores = NULL;
  info.pWaitDstStageMask = NULL;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &r->immidiate_command_buffer;
  info.signalSemaphoreCount = 0;
  info.pSignalSemaphores = NULL;

  vkres = vkQueueSubmit(r->graphics_queue, 1, &info, NULL);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

  vkres = vkQueueWaitIdle(r->graphics_queue);

  if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
    goto out;
  }

out:
  return code;
}

void owl_renderer_immidiate_command_buffer_deinit(struct owl_renderer *r) {
  OWL_ASSERT(r->immidiate_command_buffer);

  vkFreeCommandBuffers(r->device, r->transient_command_pool, 1,
                       &r->immidiate_command_buffer);

  r->immidiate_command_buffer = VK_NULL_HANDLE;
}

OWL_INTERNAL enum owl_code owl_renderer_next_image(struct owl_renderer *r) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vkres =
      vkAcquireNextImageKHR(r->device, r->swapchain, OWL_ACQUIRE_IMAGE_TIMEOUT,
                            r->active_image_available_semaphore, VK_NULL_HANDLE,
                            &r->active_swapchain_image_index);

  if (VK_ERROR_OUT_OF_DATE_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  }

  r->active_swapchain_image =
      r->swapchain_images[r->active_swapchain_image_index];

  r->active_swapchain_framebuffer =
      r->swapchain_framebuffers[r->active_swapchain_image_index];

  return code;
}

OWL_INTERNAL void owl_renderer_prepare_frame(struct owl_renderer *r) {
  OWL_VK_CHECK(vkWaitForFences(r->device, 1, &r->active_in_flight_fence,
                               VK_TRUE, OWL_WAIT_FOR_FENCES_TIMEOUT));

  OWL_VK_CHECK(vkResetFences(r->device, 1, &r->active_in_flight_fence));

  OWL_VK_CHECK(vkResetCommandPool(r->device, r->active_frame_command_pool, 0));
}

OWL_INTERNAL void owl_renderer_begin_recording(struct owl_renderer *r) {
  {
    VkCommandBufferBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    OWL_VK_CHECK(vkBeginCommandBuffer(r->active_frame_command_buffer, &info));
  }

  {
    VkRenderPassBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext = NULL;
    info.renderPass = r->main_render_pass;
    info.framebuffer = r->active_swapchain_framebuffer;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = r->swapchain_extent;
    info.clearValueCount = OWL_RENDERER_CLEAR_VALUE_COUNT;
    info.pClearValues = r->swapchain_clear_values;

    vkCmdBeginRenderPass(r->active_frame_command_buffer, &info,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
}

enum owl_code owl_renderer_frame_begin(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_next_image(r))) {
    goto out;
  }

  owl_renderer_prepare_frame(r);
  owl_renderer_begin_recording(r);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_end_recording(struct owl_renderer *r) {
  vkCmdEndRenderPass(r->active_frame_command_buffer);
  OWL_VK_CHECK(vkEndCommandBuffer(r->active_frame_command_buffer));
}

OWL_INTERNAL void owl_renderer_submit_graphics(struct owl_renderer *r) {
  VkSubmitInfo info;
  VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = NULL;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &r->active_image_available_semaphore;
  info.signalSemaphoreCount = 1;
  info.pSignalSemaphores = &r->active_render_done_semaphore;
  info.pWaitDstStageMask = &stage;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &r->active_frame_command_buffer;

  OWL_VK_CHECK(
      vkQueueSubmit(r->graphics_queue, 1, &info, r->active_in_flight_fence));
}

OWL_INTERNAL enum owl_code
owl_renderer_present_swapchain(struct owl_renderer *r) {
  VkPresentInfoKHR info;

  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = NULL;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &r->active_render_done_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &r->swapchain;
  info.pImageIndices = &r->active_swapchain_image_index;
  info.pResults = NULL;

  vkres = vkQueuePresentKHR(r->present_queue, &info);

  if (VK_ERROR_OUT_OF_DATE_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vkres) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  }

  return code;
}

OWL_INTERNAL void owl_renderer_actives_update(struct owl_renderer *r) {
  if (OWL_RENDERER_IN_FLIGHT_FRAME_COUNT == ++r->active_frame) {
    r->active_frame = 0;
  }

  r->active_in_flight_fence = r->in_flight_fences[r->active_frame];
  r->active_image_available_semaphore =
      r->image_available_semaphores[r->active_frame];
  r->active_render_done_semaphore = r->render_done_semaphores[r->active_frame];
  r->active_frame_command_buffer = r->frame_command_buffers[r->active_frame];
  r->active_frame_command_pool = r->frame_command_pools[r->active_frame];
  r->active_frame_heap_data = r->frame_heap_data[r->active_frame];
  r->active_frame_heap_buffer = r->frame_heap_buffers[r->active_frame];
  r->active_frame_heap_common_set = r->frame_heap_common_sets[r->active_frame];
  r->active_frame_heap_model1_set = r->frame_heap_model1_sets[r->active_frame];
  r->active_frame_heap_model2_set = r->frame_heap_model2_sets[r->active_frame];
}

OWL_INTERNAL void owl_renderer_time_stamps_update(struct owl_renderer *r) {
  r->time_stamp_previous = r->time_stamp_current;
  r->time_stamp_current = owl_io_time_stamp_get();
  r->time_stamp_delta = r->time_stamp_current - r->time_stamp_previous;
}

enum owl_code owl_renderer_frame_end(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  owl_renderer_end_recording(r);
  owl_renderer_submit_graphics(r);

  if (OWL_SUCCESS != (code = owl_renderer_present_swapchain(r))) {
    goto out;
  }

  owl_renderer_actives_update(r);
  owl_renderer_frame_heap_offset_clear(r);
  owl_renderer_frame_heap_garbage_clear(r);
  owl_renderer_time_stamps_update(r);

out:
  return code;
}

OWL_INTERNAL VkFormat owl_as_vk_format(enum owl_renderer_pixel_format fmt) {
  switch (fmt) {
  case OWL_RENDERER_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

OWL_INTERNAL VkFilter
owl_as_vk_filter(enum owl_renderer_sampler_filter filter) {
  switch (filter) {
  case OWL_RENDERER_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_RENDERER_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

OWL_INTERNAL VkSamplerMipmapMode
owl_as_vk_sampler_mipmap_mode(enum owl_renderer_sampler_mip_mode mode) {
  switch (mode) {
  case OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

OWL_INTERNAL VkSamplerAddressMode
owl_as_vk_sampler_addr_mode(enum owl_renderer_sampler_addr_mode mode) {
  switch (mode) {
  case OWL_RENDERER_SAMPLER_ADDR_MODE_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;

  case OWL_RENDERER_SAMPLER_ADDR_MODE_MIRRORED_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

  case OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  case OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }
}

OWL_INTERNAL owl_u64
owl_renderer_pixel_format_size(enum owl_renderer_pixel_format format) {
  switch (format) {
  case OWL_RENDERER_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof(owl_u8);

  case OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof(owl_u8);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_image_pool_find_slot(struct owl_renderer *r,
                                  owl_renderer_image_descriptor *imgd) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    /* skip all active slots */
    if (r->image_pool_slots[i]) {
      continue;
    }

    /* found unactive slot */
    *imgd = i;
    r->image_pool_slots[i] = 1;

    goto out;
  }

  /* no active slot was found */
  code = OWL_ERROR_UNKNOWN;
  *imgd = OWL_RENDERER_IMAGE_POOL_SLOT_COUNT;

out:
  return code;
}

OWL_INTERNAL owl_u32 owl_calculate_mip_levels(owl_u32 w, owl_u32 h) {
  return (owl_u32)(floor(log2(OWL_MAX(w, h))) + 1);
}

struct owl_renderer_image_transition_info {
  owl_u32 mips;
  owl_u32 layers;
  VkImageLayout src_layout;
  VkImageLayout dst_layout;
};

struct owl_renderer_image_generate_mips_info {
  owl_i32 width;
  owl_i32 height;
  owl_u32 mips;
};

OWL_INTERNAL enum owl_code owl_renderer_image_transition(
    struct owl_renderer const *r, owl_renderer_image_descriptor imgd,
    struct owl_renderer_image_transition_info const *info) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(r->immidiate_command_buffer);

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = info->src_layout;
  barrier.newLayout = info->dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = r->image_pool_images[imgd];
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = info->mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = info->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == info->src_layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->src_layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == info->src_layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ==
                 info->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == info->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == info->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == info->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkCmdPipelineBarrier(r->immidiate_command_buffer, src, dst, 0, 0, NULL, 0,
                       NULL, 1, &barrier);

out:
  return code;
}

OWL_INTERNAL enum owl_code owl_renderer_image_generate_mips(
    struct owl_renderer const *r, owl_renderer_image_descriptor imgd,
    struct owl_renderer_image_generate_mips_info const *info) {
  owl_i32 i;
  owl_i32 width;
  owl_i32 height;
  VkImageMemoryBarrier barrier;

  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(r->immidiate_command_buffer);

  if (1 == info->mips || 0 == info->mips) {
    goto out;
  }

  width = info->width;
  height = info->height;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = r->image_pool_images[imgd];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (owl_i32)(info->mips - 1); ++i) {
    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(
        r->immidiate_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    {
      VkImageBlit blit;

      blit.srcOffsets[0].x = 0;
      blit.srcOffsets[0].y = 0;
      blit.srcOffsets[0].z = 0;
      blit.srcOffsets[1].x = width;
      blit.srcOffsets[1].y = height;
      blit.srcOffsets[1].z = 1;
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = 1;
      blit.dstOffsets[0].x = 0;
      blit.dstOffsets[0].y = 0;
      blit.dstOffsets[0].z = 0;
      blit.dstOffsets[1].x = (width > 1) ? (width /= 2) : 1;
      blit.dstOffsets[1].y = (height > 1) ? (height /= 2) : 1;
      blit.dstOffsets[1].z = 1;
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel = i + 1;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = 1;

      vkCmdBlitImage(
          r->immidiate_command_buffer, r->image_pool_images[imgd],
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, r->image_pool_images[imgd],
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(r->immidiate_command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = info->mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(
      r->immidiate_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

out:
  return code;
}

struct owl_renderer_image_load_state {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  enum owl_renderer_pixel_format format;
  struct owl_renderer_frame_heap_reference reference;
};

OWL_INTERNAL enum owl_code owl_renderer_image_load_state_init(
    struct owl_renderer *r, struct owl_renderer_image_init_info const *info,
    struct owl_renderer_image_load_state *state) {
  enum owl_code code = OWL_SUCCESS;

  switch (info->src_type) {
  case OWL_RENDERER_IMAGE_SRC_TYPE_FILE: {
    owl_i32 width;
    owl_i32 height;
    owl_i32 channels;
    owl_u64 sz;
    owl_byte *data;

    data =
        stbi_load(info->src_path, &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }

    state->format = OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB;
    state->width = (owl_u32)width;
    state->height = (owl_u32)height;
    sz = state->width * state->height *
         owl_renderer_pixel_format_size(state->format);
    code = owl_renderer_frame_heap_submit(r, sz, data, &state->reference);

    stbi_image_free(data);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  } break;
  case OWL_RENDERER_IMAGE_SRC_TYPE_DATA: {
    owl_u64 sz;

    state->format = info->src_data_pixel_format;
    state->width = (owl_u32)info->src_data_width;
    state->height = (owl_u32)info->src_data_height;
    sz = state->width * state->height *
         owl_renderer_pixel_format_size(state->format);
    code = owl_renderer_frame_heap_submit(r, sz, info->src_data,
                                          &state->reference);

    if (OWL_SUCCESS != code) {
      goto out;
    }
  } break;
  }

  state->mips = owl_calculate_mip_levels(state->width, state->height);

out:
  return code;
}

void owl_renderer_image_load_state_deinit(
    struct owl_renderer *r, struct owl_renderer_image_load_state *state) {
  OWL_UNUSED(state);
  owl_renderer_frame_heap_offset_clear(r);
}

enum owl_code
owl_renderer_image_init(struct owl_renderer *r,
                        struct owl_renderer_image_init_info const *info,
                        owl_renderer_image_descriptor *imgd) {
  VkResult vkres = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;
  struct owl_renderer_image_load_state state;

  OWL_ASSERT(owl_renderer_frame_heap_offset_is_clear(r));

  code = owl_renderer_image_pool_find_slot(r, imgd);

  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_image_load_state_init(r, info, &state);

  if (OWL_SUCCESS != code) {
    goto out_err_image_pool_slot_unselect;
  }

  {
    VkImageCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = 0;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = owl_as_vk_format(state.format);
    image_info.extent.width = state.width;
    image_info.extent.height = state.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = state.mips;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkres = vkCreateImage(r->device, &image_info, NULL,
                          &r->image_pool_images[*imgd]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_image_load_state_deinit;
    }
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetImageMemoryRequirements(r->device, r->image_pool_images[*imgd],
                                 &requirements);

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
        r, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    vkres = vkAllocateMemory(r->device, &memory_info, NULL,
                             &r->image_pool_memories[*imgd]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_image_deinit;
    }

    vkres = vkBindImageMemory(r->device, r->image_pool_images[*imgd],
                              r->image_pool_memories[*imgd], 0);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_memory_deinit;
    }
  }

  {
    VkImageViewCreateInfo image_view_info;

    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.pNext = NULL;
    image_view_info.flags = 0;
    image_view_info.image = r->image_pool_images[*imgd];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = owl_as_vk_format(state.format);
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = state.mips;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    vkres = vkCreateImageView(r->device, &image_view_info, NULL,
                              &r->image_pool_image_views[*imgd]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_memory_deinit;
    }
  }

  {
    VkDescriptorSetLayout layout;
    VkDescriptorSetAllocateInfo set_info;

    layout = r->image_set_layout;

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.pNext = NULL;
    set_info.descriptorPool = r->common_set_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &layout;

    vkres = vkAllocateDescriptorSets(r->device, &set_info,
                                     &r->image_pool_sets[*imgd]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_image_view_deinit;
    }
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_init(r))) {
    goto out_err_set_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_begin(r))) {
    goto out_err_immidiate_command_buffer_deinit;
  }

  {
    struct owl_renderer_image_transition_info transition;

    transition.mips = state.mips;
    transition.layers = 1;
    transition.src_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    transition.dst_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    owl_renderer_image_transition(r, *imgd, &transition);
  }

  {
    VkBufferImageCopy copy;

    copy.bufferOffset = state.reference.offset;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x = 0;
    copy.imageOffset.y = 0;
    copy.imageOffset.z = 0;
    copy.imageExtent.width = state.width;
    copy.imageExtent.height = state.height;
    copy.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(r->immidiate_command_buffer, state.reference.buffer,
                           r->image_pool_images[*imgd],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  }

  {
    struct owl_renderer_image_generate_mips_info mips;

    mips.width = (owl_i32)state.width;
    mips.height = (owl_i32)state.height;
    mips.mips = state.mips;

    owl_renderer_image_generate_mips(r, *imgd, &mips);
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_end(r))) {
    goto out_err_immidiate_command_buffer_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_immidiate_command_buffer_submit(r))) {
    /* NOTE(samuel): if this fails, immidiate_command_buffer shouldn't be in a
     * pending state, right????? */
    goto out_err_immidiate_command_buffer_deinit;
  }

  owl_renderer_immidiate_command_buffer_deinit(r);

  {
    VkSamplerCreateInfo sampler_info;

    if (info->sampler_use_default) {
      sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_info.pNext = NULL;
      sampler_info.flags = 0;
      sampler_info.magFilter = VK_FILTER_LINEAR;
      sampler_info.minFilter = VK_FILTER_LINEAR;
      sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_info.mipLodBias = 0.0F;
      sampler_info.anisotropyEnable = VK_TRUE;
      sampler_info.maxAnisotropy = 16.0F;
      sampler_info.compareEnable = VK_FALSE;
      sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler_info.minLod = 0.0F;
      sampler_info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_info.unnormalizedCoordinates = VK_FALSE;
    } else {
      sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_info.pNext = NULL;
      sampler_info.flags = 0;
      sampler_info.magFilter = owl_as_vk_filter(info->sampler_mag_filter);
      sampler_info.minFilter = owl_as_vk_filter(info->sampler_min_filter);
      sampler_info.mipmapMode =
          owl_as_vk_sampler_mipmap_mode(info->sampler_mip_mode);
      sampler_info.addressModeU =
          owl_as_vk_sampler_addr_mode(info->sampler_wrap_u);
      sampler_info.addressModeV =
          owl_as_vk_sampler_addr_mode(info->sampler_wrap_v);
      sampler_info.addressModeW =
          owl_as_vk_sampler_addr_mode(info->sampler_wrap_w);
      sampler_info.mipLodBias = 0.0F;
      sampler_info.anisotropyEnable = VK_TRUE;
      sampler_info.maxAnisotropy = 16.0F;
      sampler_info.compareEnable = VK_FALSE;
      sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler_info.minLod = 0.0F;
      sampler_info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_info.unnormalizedCoordinates = VK_FALSE;
    }

    vkres = vkCreateSampler(r->device, &sampler_info, NULL,
                            &r->image_pool_samplers[*imgd]);

    if (OWL_SUCCESS != (code = owl_vk_result_as_owl_code(vkres))) {
      goto out_err_immidiate_command_buffer_deinit;
    }
  }

  {
    VkDescriptorImageInfo image;
    VkDescriptorImageInfo sampler;
    VkWriteDescriptorSet writes[2];

    sampler.sampler = r->image_pool_samplers[*imgd];
    sampler.imageView = VK_NULL_HANDLE;
    sampler.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image.sampler = VK_NULL_HANDLE;
    image.imageView = r->image_pool_image_views[*imgd];
    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = r->image_pool_sets[*imgd];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &sampler;
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = r->image_pool_sets[*imgd];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &image;
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(r->device, OWL_ARRAY_SIZE(writes), writes, 0, NULL);
  }

  owl_renderer_image_load_state_deinit(r, &state);

  goto out;

out_err_immidiate_command_buffer_deinit:
  owl_renderer_immidiate_command_buffer_deinit(r);

out_err_set_deinit:
  vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                       &r->image_pool_sets[*imgd]);

out_err_image_view_deinit:
  vkDestroyImageView(r->device, r->image_pool_image_views[*imgd], NULL);

out_err_memory_deinit:
  vkFreeMemory(r->device, r->image_pool_memories[*imgd], NULL);

out_err_image_deinit:
  vkDestroyImage(r->device, r->image_pool_images[*imgd], NULL);

out_err_image_load_state_deinit:
  owl_renderer_image_load_state_deinit(r, &state);

out_err_image_pool_slot_unselect:
  r->image_pool_slots[*imgd] = 0;

out:
  return code;
}

void owl_renderer_image_deinit(struct owl_renderer *r,
                               owl_renderer_image_descriptor imgd) {
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));
  OWL_ASSERT(r->image_pool_slots[imgd]);

  r->image_pool_slots[imgd] = 0;

  vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                       &r->image_pool_sets[imgd]);

  vkDestroySampler(r->device, r->image_pool_samplers[imgd], NULL);
  vkDestroyImageView(r->device, r->image_pool_image_views[imgd], NULL);
  vkFreeMemory(r->device, r->image_pool_memories[imgd], NULL);
  vkDestroyImage(r->device, r->image_pool_images[imgd], NULL);
}

void owl_renderer_active_font_set(struct owl_renderer *r,
                                  owl_renderer_font_descriptor fd) {
  r->active_font = fd;
}

OWL_INTERNAL enum owl_code owl_renderer_font_load_file(char const *path,
                                                       owl_byte **data) {
  owl_u64 sz;
  FILE *file;

  enum owl_code code = OWL_SUCCESS;

  if (!(file = fopen(path, "rb"))) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  fseek(file, 0, SEEK_END);

  sz = ftell(file);

  fseek(file, 0, SEEK_SET);

  if (!(*data = OWL_MALLOC(sz))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out_file_close;
  }

  fread(*data, sz, 1, file);

out_file_close:
  fclose(file);

out:
  return code;
}

OWL_INTERNAL void owl_renderer_font_file_unload(owl_byte *data) {
  OWL_FREE(data);
}

OWL_INTERNAL enum owl_code
owl_renderer_font_pool_find_slot(struct owl_renderer *r,
                                 owl_renderer_font_descriptor *fd) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_FONT_POOL_SLOT_COUNT; ++i) {
    /* skip all active slots */
    if (r->font_pool_slots[i]) {
      continue;
    }

    /* found unactive slot */
    *fd = i;
    r->font_pool_slots[i] = 1;

    goto out;
  }

  /* no active slot was found */
  code = OWL_ERROR_UNKNOWN;
  *fd = OWL_RENDERER_FONT_POOL_SLOT_COUNT;

out:
  return code;
}

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_FIRST_CHAR ((owl_i32)(' '))
#define OWL_FONT_CHAR_COUNT ((owl_i32)('~' - ' '))
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_WIDTH * OWL_FONT_ATLAS_HEIGHT)

OWL_STATIC_ASSERT(
    sizeof(struct owl_renderer_font_packed_char) == sizeof(stbtt_packedchar),
    "owl_font_char and stbtt_packedchar must represent the same struct");

enum owl_code
owl_renderer_font_init(struct owl_renderer *r,
                       struct owl_renderer_font_init_info const *info,
                       owl_renderer_font_descriptor *fd) {
  owl_byte *file;
  owl_byte *bitmap;
  stbtt_pack_context pack_context;
  struct owl_renderer_image_init_info image_info;

  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_font_pool_find_slot(r, fd))) {
    goto out;
  }

  if (OWL_SUCCESS != (code = owl_renderer_font_load_file(info->path, &file))) {
    goto out_err_font_slot_unselect;
  }

  if (!(bitmap = OWL_CALLOC(OWL_FONT_ATLAS_SIZE, sizeof(owl_byte)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto out_err_font_file_unload;
  }

  if (!stbtt_PackBegin(&pack_context, bitmap, OWL_FONT_ATLAS_WIDTH,
                       OWL_FONT_ATLAS_HEIGHT, 0, 1, NULL)) {
    code = OWL_ERROR_UNKNOWN;
    goto out_err_bitmap_free;
  }

  /* FIXME(samuel): hardcoded */
  stbtt_PackSetOversampling(&pack_context, 2, 2);

  /* HACK(samuel): idk if it's legal to alias a different type with the exact
   * same layout, but it _works_ so ill leave it at that*/
  if (!stbtt_PackFontRange(&pack_context, file, 0, info->size,
                           OWL_FONT_FIRST_CHAR, OWL_FONT_CHAR_COUNT,
                           (stbtt_packedchar *)(&r->font_pool_chars[*fd][0]))) {
    code = OWL_ERROR_UNKNOWN;
    goto out_err_pack_end;
  }

  image_info.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_DATA;
  image_info.src_path = NULL;
  image_info.src_data = bitmap;
  image_info.src_data_width = OWL_FONT_ATLAS_WIDTH;
  image_info.src_data_height = OWL_FONT_ATLAS_HEIGHT;
  image_info.src_data_pixel_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  image_info.sampler_use_default = 0;
  image_info.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST;
  image_info.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_info.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  image_info.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_info.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  image_info.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;

  code = owl_renderer_image_init(r, &image_info, &r->font_pool_atlases[*fd]);

  if (OWL_SUCCESS != code) {
    goto out_err_pack_end;
  }

  stbtt_PackEnd(&pack_context);

  OWL_FREE(bitmap);

  owl_renderer_font_file_unload(file);

  goto out;

out_err_pack_end:
  stbtt_PackEnd(&pack_context);

out_err_bitmap_free:
  OWL_FREE(bitmap);

out_err_font_file_unload:
  owl_renderer_font_file_unload(file);

out_err_font_slot_unselect:
  r->font_pool_slots[*fd] = 0;

out:
  return code;
}

void owl_renderer_font_deinit(struct owl_renderer *r,
                              owl_renderer_font_descriptor fd) {
  if (fd == r->active_font) {
    r->active_font = OWL_RENDERER_FONT_NONE;
  }

  r->font_pool_slots[fd] = 0;

  owl_renderer_image_deinit(r, r->font_pool_atlases[fd]);
}

enum owl_code
owl_renderer_active_font_fill_glyph(struct owl_renderer const *r, char c,
                                    owl_v2 offset,
                                    struct owl_renderer_font_glyph *glyph) {
  return owl_renderer_font_fill_glyph(r, r->active_font, c, offset, glyph);
}

enum owl_code owl_renderer_font_fill_glyph(
    struct owl_renderer const *r, owl_renderer_font_descriptor fd, char c,
    owl_v2 offset, struct owl_renderer_font_glyph *glyph) {
  stbtt_aligned_quad quad;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_RENDERER_FONT_NONE == fd) {
    OWL_ASSERT(0);
    code = OWL_ERROR_BAD_HANDLE;
    goto out;
  }

  if (OWL_RENDERER_FONT_POOL_SLOT_COUNT <= fd) {
    OWL_ASSERT(0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  stbtt_GetPackedQuad((stbtt_packedchar *)(&r->font_pool_chars[fd][0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_FONT_FIRST_CHAR, &offset[0], &offset[1], &quad,
                      1);

  OWL_V3_SET(quad.x0, quad.y0, 0.0F, glyph->positions[0]);
  OWL_V3_SET(quad.x1, quad.y0, 0.0F, glyph->positions[1]);
  OWL_V3_SET(quad.x0, quad.y1, 0.0F, glyph->positions[2]);
  OWL_V3_SET(quad.x1, quad.y1, 0.0F, glyph->positions[3]);

  OWL_V2_SET(quad.s0, quad.t0, glyph->uvs[0]);
  OWL_V2_SET(quad.s1, quad.t0, glyph->uvs[1]);
  OWL_V2_SET(quad.s0, quad.t1, glyph->uvs[2]);
  OWL_V2_SET(quad.s1, quad.t1, glyph->uvs[3]);

out:
  return code;
}

OWL_INTERNAL enum owl_code owl_renderer_font_pool_init(struct owl_renderer *r) {
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_FONT_POOL_SLOT_COUNT; ++i) {
    r->font_pool_slots[i] = 0;
  }

  r->active_font = OWL_RENDERER_FONT_NONE;

  return code;
}

OWL_INTERNAL void owl_renderer_font_pool_deinit(struct owl_renderer *r) {
  owl_i32 i;

  r->active_font = OWL_RENDERER_FONT_NONE;

  for (i = 0; i < OWL_RENDERER_FONT_POOL_SLOT_COUNT; ++i) {
    if (!r->font_pool_slots[i]) {
      continue;
    }

    r->font_pool_slots[i] = 0;
    owl_renderer_image_deinit(r, r->font_pool_atlases[i]);
  }
}

enum owl_code owl_renderer_init(struct owl_renderer_init_info const *info,
                                struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  r->time_stamp_current = 0.0;
  r->time_stamp_previous = 0.0;
  r->time_stamp_delta = 0.0;
  r->window_width = info->window_width;
  r->window_height = info->window_height;
  r->framebuffer_width = info->framebuffer_width;
  r->framebuffer_height = info->framebuffer_height;
  r->framebuffer_ratio = info->framebuffer_ratio;
  r->immidiate_command_buffer = VK_NULL_HANDLE;

  if (OWL_SUCCESS != (code = owl_renderer_instance_init(info, r))) {
    goto out;
  }

#if defined(OWL_ENABLE_VALIDATION)
  if (OWL_SUCCESS != (code = owl_renderer_debug_messenger_init(r))) {
    goto out_err_instance_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_surface_init(info, r))) {
    goto out_err_debug_messenger_deinit;
  }
#else  /* OWL_ENABLE_VALIDATION */
  if (OWL_SUCCESS != (code = owl_renderer_surface_init(info, r))) {
    goto out_err_instance_deinit;
  }
#endif /* OWL_ENABLE_VALIDATION */

  if (OWL_SUCCESS != (code = owl_renderer_device_init(r))) {
    goto out_err_surface_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_init(info, r))) {
    goto out_err_device_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_image_views_init(r))) {
    goto out_err_swapchain_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_pools_init(r))) {
    goto out_err_swapchain_image_views_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_main_render_pass_init(r))) {
    goto out_err_pools_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_attachments_init(r))) {
    goto out_err_main_render_pass_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_swapchain_framebuffers_init(r))) {
    goto out_err_attachments_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_set_layouts_init(r))) {
    goto out_err_swapchain_framebuffers_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_pipeline_layouts_init(r))) {
    goto out_err_set_layouts_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_shaders_init(r))) {
    goto out_err_pipeline_layouts_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_pipelines_init(r))) {
    goto out_err_shaders_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_frame_commands_init(r))) {
    goto out_err_pipelines_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_frame_sync_init(r))) {
    goto out_err_frame_commands_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_garbage_init(r))) {
    goto out_err_frame_sync_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_frame_heap_init(r, 1 << 24))) {
    goto out_err_frame_heap_garbage_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_image_pool_init(r))) {
    goto out_err_frame_heap_deinit;
  }

  if (OWL_SUCCESS != (code = owl_renderer_font_pool_init(r))) {
    goto out_err_image_pool_deinit;
  }

  {
    owl_renderer_font_descriptor font;
    struct owl_renderer_font_init_info font_info;

    font_info.path = "../../assets/Inconsolata-Regular.ttf";
    font_info.size = 64.0F;

    if (OWL_SUCCESS != (code = owl_renderer_font_init(r, &font_info, &font))) {
      goto out_err_font_pool_deinit;
    }

    owl_renderer_active_font_set(r, font);
  }

  goto out;

out_err_font_pool_deinit:
  owl_renderer_font_pool_deinit(r);

out_err_image_pool_deinit:
  owl_renderer_image_pool_deinit(r);

out_err_frame_heap_deinit:
  owl_renderer_frame_heap_deinit(r);

out_err_frame_heap_garbage_deinit:
  owl_renderer_frame_heap_garbage_deinit(r);

out_err_frame_sync_deinit:
  owl_renderer_frame_sync_deinit(r);

out_err_frame_commands_deinit:
  owl_renderer_frame_commands_deinit(r);

out_err_pipelines_deinit:
  owl_renderer_pipelines_deinit(r);

out_err_shaders_deinit:
  owl_renderer_shaders_deinit(r);

out_err_pipeline_layouts_deinit:
  owl_renderer_pipeline_layouts_deinit(r);

out_err_set_layouts_deinit:
  owl_renderer_set_layouts_deinit(r);

out_err_swapchain_framebuffers_deinit:
  owl_renderer_swapchain_framebuffers_deinit(r);

out_err_attachments_deinit:
  owl_renderer_attachments_deinit(r);

out_err_main_render_pass_deinit:
  owl_renderer_main_render_pass_deinit(r);

out_err_pools_deinit:
  owl_renderer_pools_deinit(r);

out_err_swapchain_image_views_deinit:
  owl_renderer_swapchain_image_views_deinit(r);

out_err_swapchain_deinit:
  owl_renderer_swapchain_deinit(r);

out_err_device_deinit:
  owl_renderer_device_deinit(r);

out_err_surface_deinit:
  owl_renderer_surface_deinit(r);

#if defined(OWL_ENABLE_VALIDATION)
out_err_debug_messenger_deinit:
  owl_renderer_debug_messenger_deinit(r);
#endif /* OWL_ENABLE_VALIDATION */

out_err_instance_deinit:
  owl_renderer_instance_deinit(r);

out:
  return code;
}

void owl_renderer_deinit(struct owl_renderer *r) {
  OWL_VK_CHECK(vkDeviceWaitIdle(r->device));

  owl_renderer_font_pool_deinit(r);
  owl_renderer_image_pool_deinit(r);
  owl_renderer_frame_heap_deinit(r);
  owl_renderer_frame_heap_garbage_deinit(r);
  owl_renderer_frame_sync_deinit(r);
  owl_renderer_frame_commands_deinit(r);
  owl_renderer_pipelines_deinit(r);
  owl_renderer_shaders_deinit(r);
  owl_renderer_pipeline_layouts_deinit(r);
  owl_renderer_set_layouts_deinit(r);
  owl_renderer_swapchain_framebuffers_deinit(r);
  owl_renderer_attachments_deinit(r);
  owl_renderer_main_render_pass_deinit(r);
  owl_renderer_pools_deinit(r);
  owl_renderer_swapchain_image_views_deinit(r);
  owl_renderer_swapchain_deinit(r);
  owl_renderer_device_deinit(r);
  owl_renderer_surface_deinit(r);

#if defined(OWL_ENABLE_VALIDATION)
  owl_renderer_debug_messenger_deinit(r);
#endif /* OWL_ENABLE_VALIDATION */

  owl_renderer_instance_deinit(r);
}

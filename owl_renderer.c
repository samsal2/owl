#include "owl_renderer.h"

#include "stb_image.h"
#include "owl_client.h"
#include "owl_draw_command.h"
#include "owl_internal.h"
#include "owl_model.h"

#include <math.h>

#define OWL_MAX_PRESENT_MODES 16
#define OWL_UNRESTRICTED_DIMENSION (owl_u32) - 1
#define OWL_ACQUIRE_IMAGE_TIMEOUT (owl_u64) - 1
#define OWL_WAIT_FOR_FENCES_TIMEOUT (owl_u64) - 1
#define OWL_QUEUE_FAMILY_INDEX_NONE (owl_u32) - 1

OWL_GLOBAL char const *const g_required_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(OWL_ENABLE_VALIDATION)

OWL_GLOBAL char const *const g_debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif /* OWL_ENABLE_VALIDATION */

OWL_INTERNAL enum owl_code
owl_renderer_init_instance_(struct owl_renderer_init_desc const *desc,
                            struct owl_renderer *renderer) {
  VkApplicationInfo app_info;
  VkInstanceCreateInfo instance_create_info;
  enum owl_code code = OWL_SUCCESS;

  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = NULL;
  app_info.pApplicationName = desc->name;
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = NULL;
  instance_create_info.flags = 0;
  instance_create_info.pApplicationInfo = &app_info;
#if defined(OWL_ENABLE_VALIDATION)
  instance_create_info.enabledLayerCount =
      OWL_ARRAY_SIZE(g_debug_validation_layers);
  instance_create_info.ppEnabledLayerNames = g_debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
  instance_create_info.enabledLayerCount = 0;
  instance_create_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
  instance_create_info.enabledExtensionCount =
      (owl_u32)desc->instance_extensions_count;
  instance_create_info.ppEnabledExtensionNames = desc->instance_extensions;

  OWL_VK_CHECK(
      vkCreateInstance(&instance_create_info, NULL, &renderer->instance));

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_instance_(struct owl_renderer *renderer) {
  vkDestroyInstance(renderer->instance, NULL);
}

#if defined(OWL_ENABLE_VALIDATION)

#define OWL_VK_GET_INSTANCE_PROC_ADDR(i, fn)                                   \
  ((PFN_##fn)vkGetInstanceProcAddr((i), #fn))

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32 owl_vk_debug_callback_(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user_data) {
  struct owl_renderer const *renderer = user_data;

  OWL_UNUSED(type);
  OWL_UNUSED(renderer);

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[34m(VERBOSE)\033[0m %s\n",
            data->pMessage);
  else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(INFO)\033[0m %s\n",
            data->pMessage);
  else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(WARNING)\033[0m %s\n",
            data->pMessage);
  else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    fprintf(stderr,
            "\033[32m[VALIDATION_LAYER]\033[0m \033[31m(ERROR)\033[0m %s\n",
            data->pMessage);

  return VK_FALSE;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_debug_(struct owl_renderer *renderer) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  enum owl_code code = OWL_SUCCESS;

  vkCreateDebugUtilsMessengerEXT = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkCreateDebugUtilsMessengerEXT);

  OWL_ASSERT(vkCreateDebugUtilsMessengerEXT);

  debug_messenger_create_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_create_info.pNext = NULL;
  debug_messenger_create_info.flags = 0;
  debug_messenger_create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_messenger_create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_messenger_create_info.pfnUserCallback = owl_vk_debug_callback_;
  debug_messenger_create_info.pUserData = renderer;

  OWL_VK_CHECK(vkCreateDebugUtilsMessengerEXT(
      renderer->instance, &debug_messenger_create_info, NULL,
      &renderer->debug_messenger));

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_debug_(struct owl_renderer *renderer) {
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

  vkDestroyDebugUtilsMessengerEXT = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkDestroyDebugUtilsMessengerEXT);

  OWL_ASSERT(vkDestroyDebugUtilsMessengerEXT);

  vkDestroyDebugUtilsMessengerEXT(renderer->instance, renderer->debug_messenger,
                                  NULL);
}

#endif /* OWL_ENABLE_VALIDATION */

OWL_INTERNAL enum owl_code
owl_renderer_init_surface_(struct owl_renderer_init_desc const *desc,
                           struct owl_renderer *renderer) {
  return desc->create_surface(renderer, desc->surface_user_data,
                              &renderer->surface);
}

OWL_INTERNAL void owl_renderer_deinit_surface_(struct owl_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_fill_device_options_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  OWL_VK_CHECK(vkEnumeratePhysicalDevices(
      renderer->instance, &renderer->device_options_count, NULL));

  if (OWL_RENDERER_MAX_DEVICE_OPTIONS_COUNT <= renderer->device_options_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance,
                                          &renderer->device_options_count,
                                          renderer->device_options));

end:
  return code;
}

OWL_INTERNAL
owl_b32 owl_renderer_query_families_(struct owl_renderer const *renderer,
                                     owl_u32 *graphics_family_index,
                                     owl_u32 *present_family_index) {
  owl_i32 found;
  owl_u32 i;
  owl_u32 properties_count;
  VkQueueFamilyProperties *properties;

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &properties_count, NULL);

  if (!(properties = OWL_MALLOC(properties_count * sizeof(*properties)))) {
    found = 0;
    goto end;
  }

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &properties_count, properties);

  *graphics_family_index = OWL_QUEUE_FAMILY_INDEX_NONE;
  *present_family_index = OWL_QUEUE_FAMILY_INDEX_NONE;

  for (i = 0; i < properties_count; ++i) {
    VkBool32 has_surface;

    if (!properties[i].queueCount)
      continue;

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags)
      *graphics_family_index = i;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
        renderer->physical_device, i, renderer->surface, &has_surface));

    if (has_surface)
      *present_family_index = i;

    if (OWL_QUEUE_FAMILY_INDEX_NONE == *graphics_family_index)
      continue;

    if (OWL_QUEUE_FAMILY_INDEX_NONE == *present_family_index)
      continue;

    found = 1;
    goto end_free_properties;
  }

  found = 0;

end_free_properties:
  OWL_FREE(properties);

end:
  return found;
}

OWL_INTERNAL int
owl_validate_device_extensions_(owl_u32 extensions_count,
                                VkExtensionProperties const *extensions) {
  owl_u32 i;
  owl_i32 found = 1;
  owl_b32 extensions_found[OWL_ARRAY_SIZE(g_required_device_extensions)];

  OWL_MEMSET(extensions_found, 0, sizeof(extensions_found));

  for (i = 0; i < extensions_count; ++i) {
    owl_u32 j;
    for (j = 0; j < OWL_ARRAY_SIZE(g_required_device_extensions); ++j) {
      if (!OWL_STRNCMP(g_required_device_extensions[j],
                       extensions[i].extensionName,
                       VK_MAX_EXTENSION_NAME_SIZE)) {
        extensions_found[j] = 1;
      }
    }
  }

  for (i = 0; i < OWL_ARRAY_SIZE(g_required_device_extensions); ++i) {
    if (!extensions_found[i]) {
      found = 0;
      goto end;
    }
  }

end:
  return found;
}

OWL_INTERNAL enum owl_code
owl_renderer_select_physical_device_(struct owl_renderer *renderer) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < renderer->device_options_count; ++i) {
    owl_u32 has_formats;
    owl_u32 has_modes;
    owl_u32 extension_count;
    VkExtensionProperties *extensions;

    renderer->physical_device = renderer->device_options[i];

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        renderer->physical_device, renderer->surface, &has_formats, NULL));

    if (!has_formats)
      continue;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        renderer->physical_device, renderer->surface, &has_modes, NULL));

    if (!has_modes)
      continue;

    if (!owl_renderer_query_families_(renderer,
                                      &renderer->graphics_queue_family_index,
                                      &renderer->present_queue_family_index))
      continue;

    vkGetPhysicalDeviceFeatures(renderer->physical_device,
                                &renderer->device_features);

    if (!renderer->device_features.samplerAnisotropy)
      continue;

    OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
        renderer->physical_device, NULL, &extension_count, NULL));

    if (!(extensions = OWL_MALLOC(extension_count * sizeof(*extensions)))) {
      code = OWL_ERROR_BAD_ALLOC;
      goto end;
    }

    OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
        renderer->physical_device, NULL, &extension_count, extensions));

    if (!owl_validate_device_extensions_(extension_count, extensions)) {
      OWL_FREE(extensions);
      continue;
    }

    OWL_FREE(extensions);

    vkGetPhysicalDeviceProperties(renderer->physical_device,
                                  &renderer->device_properties);

    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device,
                                        &renderer->device_memory_properties);
    code = OWL_SUCCESS;
    goto end;
  }

  code = OWL_ERROR_NO_SUITABLE_DEVICE;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_ensure_surface_format_(struct owl_renderer const *renderer) {
  owl_u32 i;
  owl_u32 formats_count;
  VkSurfaceFormatKHR *formats;
  enum owl_code code = OWL_SUCCESS;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &formats_count, NULL));

  if (!(formats = OWL_MALLOC(formats_count * sizeof(*formats)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &formats_count, formats));

  for (i = 0; i < formats_count; ++i) {
    if (renderer->surface_format.format != formats[i].format)
      continue;

    if (renderer->surface_format.colorSpace != formats[i].colorSpace)
      continue;

    code = OWL_SUCCESS;
    goto end_free_formats;
  }

  /* if the loop ends without finding a format */
  code = OWL_ERROR_NO_SUITABLE_FORMAT;

end_free_formats:
  OWL_FREE(formats);

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_ensure_msaa_sample_count_(struct owl_renderer const *renderer) {
  enum owl_code code = OWL_SUCCESS;
  VkSampleCountFlags const supported_samples =
      renderer->device_properties.limits.framebufferColorSampleCounts &
      renderer->device_properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_1_BIT & renderer->msaa_sample_count) {
    OWL_ASSERT(0 && "disabling multisampling is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  if (!(supported_samples & renderer->msaa_sample_count)) {
    OWL_ASSERT(0 && "msaa_sample_count is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

end:
  return code;
}

OWL_INTERNAL owl_u32
owl_renderer_get_queue_count_(struct owl_renderer const *renderer) {
  if (renderer->graphics_queue_family_index ==
      renderer->present_queue_family_index)
    return 1;
  else
    return 2;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_device_(struct owl_renderer *renderer) {
  VkDeviceCreateInfo device_create_info;
  VkDeviceQueueCreateInfo queues_create_infos[2];
  float const priority = 1.0F;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_fill_device_options_(renderer)))
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_select_physical_device_(renderer)))
    goto end;

  renderer->surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
  renderer->surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  if (OWL_SUCCESS != (code = owl_renderer_ensure_surface_format_(renderer)))
    goto end;

  /* TODO(samuel): add and option for choosing this */
  renderer->msaa_sample_count = VK_SAMPLE_COUNT_2_BIT;

  if (OWL_SUCCESS != (code = owl_renderer_ensure_msaa_sample_count_(renderer)))
    goto end;

  queues_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues_create_infos[0].pNext = NULL;
  queues_create_infos[0].flags = 0;
  queues_create_infos[0].queueFamilyIndex =
      renderer->graphics_queue_family_index;
  queues_create_infos[0].queueCount = 1;
  queues_create_infos[0].pQueuePriorities = &priority;

  queues_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues_create_infos[1].pNext = NULL;
  queues_create_infos[1].flags = 0;
  queues_create_infos[1].queueFamilyIndex =
      renderer->present_queue_family_index;
  queues_create_infos[1].queueCount = 1;
  queues_create_infos[1].pQueuePriorities = &priority;

  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = NULL;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount =
      owl_renderer_get_queue_count_(renderer);
  device_create_info.pQueueCreateInfos = queues_create_infos;
  device_create_info.enabledLayerCount = 0;      /* deprecated */
  device_create_info.ppEnabledLayerNames = NULL; /* deprecated */
  device_create_info.enabledExtensionCount =
      OWL_ARRAY_SIZE(g_required_device_extensions);
  device_create_info.ppEnabledExtensionNames = g_required_device_extensions;
  device_create_info.pEnabledFeatures = &renderer->device_features;

  OWL_VK_CHECK(vkCreateDevice(renderer->physical_device, &device_create_info,
                              NULL, &renderer->device));

  vkGetDeviceQueue(renderer->device, renderer->graphics_queue_family_index, 0,
                   &renderer->graphics_queue);
  vkGetDeviceQueue(renderer->device, renderer->present_queue_family_index, 0,
                   &renderer->present_queue);

end:
  return code;
}

OWL_INTERNAL void owl_renderer_deinit_device_(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

OWL_INTERNAL void
owl_renderer_clamp_swapchain_extent_(struct owl_renderer *renderer) {
  VkSurfaceCapabilitiesKHR capabilities;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      renderer->physical_device, renderer->surface, &capabilities));

  if (OWL_UNRESTRICTED_DIMENSION == capabilities.currentExtent.width) {
    renderer->swapchain_extent = capabilities.currentExtent;
  } else {
    owl_u32 const min_width = capabilities.minImageExtent.width;
    owl_u32 const min_height = capabilities.minImageExtent.height;
    owl_u32 const max_width = capabilities.maxImageExtent.width;
    owl_u32 const max_height = capabilities.maxImageExtent.height;

    renderer->swapchain_extent.width =
        OWL_CLAMP(renderer->swapchain_extent.width, min_width, max_width);
    renderer->swapchain_extent.height =
        OWL_CLAMP(renderer->swapchain_extent.height, min_height, max_height);
  }
}

OWL_INTERNAL enum owl_code
owl_renderer_ensure_present_mode_(struct owl_renderer *renderer) {
  owl_u32 i;
  owl_u32 modes_count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];
  enum owl_code code = OWL_SUCCESS;
  VkPresentModeKHR const prefered = renderer->swapchain_present_mode;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &modes_count, NULL));

  if (OWL_MAX_PRESENT_MODES <= modes_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &modes_count, modes));

  for (i = 0; i < modes_count; ++i)
    if (prefered ==
        (renderer->swapchain_present_mode = modes[modes_count - i - 1]))
      break;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_swapchain_(struct owl_renderer_init_desc const *desc,
                             struct owl_renderer *renderer) {
  owl_u32 families[2];
  VkSurfaceCapabilitiesKHR capabilities;
  VkSwapchainCreateInfoKHR swapchain_create_info;
  enum owl_code code = OWL_SUCCESS;

  families[0] = renderer->graphics_queue_family_index;
  families[1] = renderer->present_queue_family_index;

  renderer->swapchain_extent.width = (owl_u32)desc->framebuffer_width;
  renderer->swapchain_extent.height = (owl_u32)desc->framebuffer_height;
  owl_renderer_clamp_swapchain_extent_(renderer);

  renderer->swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  owl_renderer_ensure_present_mode_(renderer);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      renderer->physical_device, renderer->surface, &capabilities));

  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = NULL;
  swapchain_create_info.flags = 0;
  swapchain_create_info.surface = renderer->surface;
  swapchain_create_info.minImageCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
  swapchain_create_info.imageFormat = renderer->surface_format.format;
  swapchain_create_info.imageColorSpace = renderer->surface_format.colorSpace;
  swapchain_create_info.imageExtent.width = renderer->swapchain_extent.width;
  swapchain_create_info.imageExtent.height = renderer->swapchain_extent.height;
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.preTransform = capabilities.currentTransform;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = renderer->swapchain_present_mode;
  swapchain_create_info.clipped = VK_TRUE;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

  if (renderer->graphics_queue_family_index ==
      renderer->present_queue_family_index) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = NULL;
  } else {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount =
        owl_renderer_get_queue_count_(renderer);
    swapchain_create_info.pQueueFamilyIndices = families;
  }

  OWL_VK_CHECK(vkCreateSwapchainKHR(renderer->device, &swapchain_create_info,
                                    NULL, &renderer->swapchain));

  OWL_VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                       &renderer->swapchain_images_count,
                                       NULL));

  if (OWL_RENDERER_MAX_SWAPCHAIN_IMAGES_COUNT <=
      renderer->swapchain_images_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                       &renderer->swapchain_images_count,
                                       renderer->swapchain_images));

  renderer->swapchain_clear_values[0].color.float32[0] = 0.0F;
  renderer->swapchain_clear_values[0].color.float32[1] = 0.0F;
  renderer->swapchain_clear_values[0].color.float32[2] = 0.0F;
  renderer->swapchain_clear_values[0].color.float32[3] = 1.0F;
  renderer->swapchain_clear_values[1].depthStencil.depth = 1.0F;
  renderer->swapchain_clear_values[1].depthStencil.stencil = 0.0F;

end:
  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_swapchain_(struct owl_renderer *renderer) {
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_swapchain_views_(struct owl_renderer *renderer) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < renderer->swapchain_images_count; ++i) {
    VkImageViewCreateInfo view_create_info;

    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.flags = 0;
    view_create_info.image = renderer->swapchain_images[i];
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = renderer->surface_format.format;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view_create_info, NULL,
                                   &renderer->swapchain_views[i]));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_swapchain_views_(struct owl_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < renderer->swapchain_images_count; ++i)
    vkDestroyImageView(renderer->device, renderer->swapchain_views[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_common_pools_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkCommandPoolCreateInfo pool_create_info;

    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.pNext = NULL;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_create_info.queueFamilyIndex = renderer->graphics_queue_family_index;

    OWL_VK_CHECK(vkCreateCommandPool(renderer->device, &pool_create_info, NULL,
                                     &renderer->transient_command_pool));
  }

  {
    /* NOTE: 256 ought to be enough... */
    VkDescriptorPoolSize sizes[6];
    VkDescriptorPoolCreateInfo pool_create_info;

    sizes[0].descriptorCount = 256;
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    sizes[1].descriptorCount = 256;
    sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

    sizes[2].descriptorCount = 256;
    sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    sizes[3].descriptorCount = 256;
    sizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    sizes[4].descriptorCount = 256;
    sizes[4].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    sizes[5].descriptorCount = 256;
    sizes[5].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

    pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_create_info.pNext = NULL;
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_create_info.maxSets = 80;
    pool_create_info.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    pool_create_info.pPoolSizes = sizes;

    OWL_VK_CHECK(vkCreateDescriptorPool(renderer->device, &pool_create_info,
                                        NULL, &renderer->common_set_pool));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_common_pools_(struct owl_renderer *renderer) {
  vkDestroyDescriptorPool(renderer->device, renderer->common_set_pool, NULL);
  vkDestroyCommandPool(renderer->device, renderer->transient_command_pool,
                       NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_select_depth_stencil_format_(struct owl_renderer *renderer) {
  VkFormatProperties properties;
  enum owl_code code = OWL_SUCCESS;

  vkGetPhysicalDeviceFormatProperties(renderer->physical_device,
                                      VK_FORMAT_D24_UNORM_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    renderer->depth_stencil_format = VK_FORMAT_D24_UNORM_S8_UINT;
    goto end;
  }

  vkGetPhysicalDeviceFormatProperties(
      renderer->physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    renderer->depth_stencil_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    goto end;
  }

  code = OWL_ERROR_UNKNOWN;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_main_render_pass_(struct owl_renderer *renderer) {
  VkAttachmentReference color_attachment_reference;
  VkAttachmentReference depth_attachment_reference;
  VkAttachmentReference resolve_attachment_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo render_pass_create_info;
  enum owl_code code = OWL_SUCCESS;

  code = owl_renderer_select_depth_stencil_format_(renderer);

  if (OWL_SUCCESS != code)
    goto end;

  color_attachment_reference.attachment = 0;
  color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  depth_attachment_reference.attachment = 1;
  depth_attachment_reference.layout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  resolve_attachment_reference.attachment = 2;
  resolve_attachment_reference.layout =
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  attachments[0].flags = 0;
  attachments[0].format = renderer->surface_format.format;
  attachments[0].samples = renderer->msaa_sample_count;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  attachments[1].flags = 0;
  attachments[1].format = renderer->depth_stencil_format;
  attachments[1].samples = renderer->msaa_sample_count;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  attachments[2].flags = 0;
  attachments[2].format = renderer->surface_format.format;
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
  subpass.pColorAttachments = &color_attachment_reference;
  subpass.pResolveAttachments = &resolve_attachment_reference;
  subpass.pDepthStencilAttachment = &depth_attachment_reference;
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

  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.pNext = NULL;
  render_pass_create_info.flags = 0;
  render_pass_create_info.attachmentCount = OWL_ARRAY_SIZE(attachments);
  render_pass_create_info.pAttachments = attachments;
  render_pass_create_info.subpassCount = 1;
  render_pass_create_info.pSubpasses = &subpass;
  render_pass_create_info.dependencyCount = 1;
  render_pass_create_info.pDependencies = &dependency;

  OWL_VK_CHECK(vkCreateRenderPass(renderer->device, &render_pass_create_info,
                                  NULL, &renderer->main_render_pass));

end:
  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_main_render_pass_(struct owl_renderer *renderer) {
  vkDestroyRenderPass(renderer->device, renderer->main_render_pass, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_attachments_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = renderer->surface_format.format;
    image_create_info.extent.width = renderer->swapchain_extent.width;
    image_create_info.extent.height = renderer->swapchain_extent.height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = renderer->msaa_sample_count;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(renderer->device, &image_create_info, NULL,
                               &renderer->color_image));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetImageMemoryRequirements(renderer->device, renderer->color_image,
                                 &requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                  &renderer->color_memory));

    OWL_VK_CHECK(vkBindImageMemory(renderer->device, renderer->color_image,
                                   renderer->color_memory, 0));
  }

  {
    VkImageViewCreateInfo view_create_info;

    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.flags = 0;
    view_create_info.image = renderer->color_image;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = renderer->surface_format.format;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view_create_info, NULL,
                                   &renderer->color_view));
  }

  {
    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = renderer->depth_stencil_format;
    image_create_info.extent.width = renderer->swapchain_extent.width;
    image_create_info.extent.height = renderer->swapchain_extent.height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = renderer->msaa_sample_count;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(renderer->device, &image_create_info, NULL,
                               &renderer->depth_stencil_image));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetImageMemoryRequirements(renderer->device,
                                 renderer->depth_stencil_image, &requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                  &renderer->depth_stencil_memory));

    OWL_VK_CHECK(vkBindImageMemory(renderer->device,
                                   renderer->depth_stencil_image,
                                   renderer->depth_stencil_memory, 0));
  }

  {
    VkImageViewCreateInfo view_create_info;

    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.flags = 0;
    view_create_info.image = renderer->depth_stencil_image;
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format = renderer->depth_stencil_format;
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = 1;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view_create_info, NULL,
                                   &renderer->depth_stencil_view));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_attachments_(struct owl_renderer *renderer) {
  vkDestroyImageView(renderer->device, renderer->depth_stencil_view, NULL);
  vkFreeMemory(renderer->device, renderer->depth_stencil_memory, NULL);
  vkDestroyImage(renderer->device, renderer->depth_stencil_image, NULL);

  vkDestroyImageView(renderer->device, renderer->color_view, NULL);
  vkFreeMemory(renderer->device, renderer->color_memory, NULL);
  vkDestroyImage(renderer->device, renderer->color_image, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_swapchain_framebuffers_(struct owl_renderer *renderer) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < renderer->swapchain_images_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo framebuffer_create_info;

    attachments[0] = renderer->color_view;
    attachments[1] = renderer->depth_stencil_view;
    attachments[2] = renderer->swapchain_views[i];

    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.pNext = NULL;
    framebuffer_create_info.flags = 0;
    framebuffer_create_info.renderPass = renderer->main_render_pass;
    framebuffer_create_info.attachmentCount = OWL_ARRAY_SIZE(attachments);
    framebuffer_create_info.pAttachments = attachments;
    framebuffer_create_info.width = renderer->swapchain_extent.width;
    framebuffer_create_info.height = renderer->swapchain_extent.height;
    framebuffer_create_info.layers = 1;

    OWL_VK_CHECK(vkCreateFramebuffer(renderer->device, &framebuffer_create_info,
                                     NULL,
                                     &renderer->swapchain_framebuffers[i]));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_swapchain_framebuffers_(struct owl_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < renderer->swapchain_images_count; ++i)
    vkDestroyFramebuffer(renderer->device, renderer->swapchain_framebuffers[i],
                         NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_set_layouts_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.pNext = NULL;
    set_layout_create_info.flags = 0;
    set_layout_create_info.bindingCount = 1;
    set_layout_create_info.pBindings = &binding;

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device,
                                             &set_layout_create_info, NULL,
                                             &renderer->vertex_ubo_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.pNext = NULL;
    set_layout_create_info.flags = 0;
    set_layout_create_info.bindingCount = 1;
    set_layout_create_info.pBindings = &binding;

    OWL_VK_CHECK(
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_create_info,
                                    NULL, &renderer->fragment_ubo_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.pNext = NULL;
    set_layout_create_info.flags = 0;
    set_layout_create_info.bindingCount = 1;
    set_layout_create_info.pBindings = &binding;

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device,
                                             &set_layout_create_info, NULL,
                                             &renderer->shared_ubo_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.pNext = NULL;
    set_layout_create_info.flags = 0;
    set_layout_create_info.bindingCount = 1;
    set_layout_create_info.pBindings = &binding;

    OWL_VK_CHECK(
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_create_info,
                                    NULL, &renderer->vertex_ssbo_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo set_layout_create_info;

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

    set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_create_info.pNext = NULL;
    set_layout_create_info.flags = 0;
    set_layout_create_info.bindingCount = OWL_ARRAY_SIZE(bindings);
    set_layout_create_info.pBindings = bindings;

    OWL_VK_CHECK(
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_create_info,
                                    NULL, &renderer->sampler_image_set_layout));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_set_layouts_(struct owl_renderer *renderer) {
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->sampler_image_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->vertex_ssbo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->shared_ubo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->fragment_ubo_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->vertex_ubo_set_layout, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_pipeline_layouts_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  renderer->active_pipeline = VK_NULL_HANDLE;
  renderer->active_pipeline_layout = VK_NULL_HANDLE;

  {
    VkDescriptorSetLayout sets[2];
    VkPipelineLayoutCreateInfo pipeline_layout_create_info;

    sets[0] = renderer->vertex_ubo_set_layout;
    sets[1] = renderer->sampler_image_set_layout;

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = NULL;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount = OWL_ARRAY_SIZE(sets);
    pipeline_layout_create_info.pSetLayouts = sets;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = NULL;

    OWL_VK_CHECK(vkCreatePipelineLayout(renderer->device,
                                        &pipeline_layout_create_info, NULL,
                                        &renderer->common_pipeline_layout));
  }

  {
    VkPushConstantRange push_constant;
    VkDescriptorSetLayout sets[5];
    VkPipelineLayoutCreateInfo pipeline_layout_create_info;

    push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(struct owl_model_material_push_constant);

    sets[0] = renderer->shared_ubo_set_layout;
    sets[1] = renderer->sampler_image_set_layout;
    sets[2] = renderer->sampler_image_set_layout;
    sets[3] = renderer->vertex_ssbo_set_layout;
    sets[4] = renderer->fragment_ubo_set_layout;

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = NULL;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount = OWL_ARRAY_SIZE(sets);
    pipeline_layout_create_info.pSetLayouts = sets;
    pipeline_layout_create_info.pushConstantRangeCount = 1;
    pipeline_layout_create_info.pPushConstantRanges = &push_constant;

    OWL_VK_CHECK(vkCreatePipelineLayout(renderer->device,
                                        &pipeline_layout_create_info, NULL,
                                        &renderer->model_pipeline_layout));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_pipeline_layouts_(struct owl_renderer *renderer) {
  vkDestroyPipelineLayout(renderer->device, renderer->model_pipeline_layout,
                          NULL);
  vkDestroyPipelineLayout(renderer->device, renderer->common_pipeline_layout,
                          NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_shaders_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo shader_create_info;

    /* TODO(samuel): Properly load code at runtime */
    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "owl_shader_basic.vert.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(code);
    shader_create_info.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader_create_info,
                                      NULL, &renderer->basic_vertex_shader));
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "owl_shader_basic.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(code);
    shader_create_info.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader_create_info,
                                      NULL, &renderer->basic_fragment_shader));
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "owl_shader_font.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(code);
    shader_create_info.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader_create_info,
                                      NULL, &renderer->font_fragment_shader));
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "owl_shader_model.vert.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(code);
    shader_create_info.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader_create_info,
                                      NULL, &renderer->model_vertex_shader));
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "owl_shader_model.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(code);
    shader_create_info.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader_create_info,
                                      NULL, &renderer->model_fragment_shader));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_shaders_(struct owl_renderer *renderer) {
  vkDestroyShaderModule(renderer->device, renderer->model_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->model_vertex_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->font_fragment_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_vertex_shader, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_pipelines_(struct owl_renderer *renderer) {

  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_PIPELINE_TYPE_COUNT; ++i) {
    VkVertexInputBindingDescription vertex_binding_descriptions[8];
    VkVertexInputAttributeDescription vertex_attr_descriptions[8];
    VkPipelineVertexInputStateCreateInfo vertex_input_state;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    VkPipelineMultisampleStateCreateInfo multisample_state;
    VkPipelineColorBlendAttachmentState color_blend_attachment_states[8];
    VkPipelineColorBlendStateCreateInfo color_blend_state;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_create_info;

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      vertex_binding_descriptions[0].binding = 0;
      vertex_binding_descriptions[0].stride =
          sizeof(struct owl_draw_command_vertex);
      vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attr_descriptions[0].binding = 0;
      vertex_attr_descriptions[0].location = 0;
      vertex_attr_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attr_descriptions[0].offset =
          offsetof(struct owl_draw_command_vertex, position);

      vertex_attr_descriptions[1].binding = 0;
      vertex_attr_descriptions[1].location = 1;
      vertex_attr_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attr_descriptions[1].offset =
          offsetof(struct owl_draw_command_vertex, color);

      vertex_attr_descriptions[2].binding = 0;
      vertex_attr_descriptions[2].location = 2;
      vertex_attr_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attr_descriptions[2].offset =
          offsetof(struct owl_draw_command_vertex, uv);
      break;

    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      vertex_binding_descriptions[0].binding = 0;
      vertex_binding_descriptions[0].stride = sizeof(struct owl_model_vertex);
      vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_binding_descriptions[0].binding = 0;
      vertex_binding_descriptions[0].stride = sizeof(struct owl_model_vertex);
      vertex_binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attr_descriptions[0].binding = 0;
      vertex_attr_descriptions[0].location = 0;
      vertex_attr_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attr_descriptions[0].offset =
          offsetof(struct owl_model_vertex, position);

      vertex_attr_descriptions[1].binding = 0;
      vertex_attr_descriptions[1].location = 1;
      vertex_attr_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attr_descriptions[1].offset =
          offsetof(struct owl_model_vertex, normal);

      vertex_attr_descriptions[2].binding = 0;
      vertex_attr_descriptions[2].location = 2;
      vertex_attr_descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attr_descriptions[2].offset =
          offsetof(struct owl_model_vertex, uv0);

      vertex_attr_descriptions[3].binding = 0;
      vertex_attr_descriptions[3].location = 3;
      vertex_attr_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attr_descriptions[3].offset =
          offsetof(struct owl_model_vertex, uv1);

      vertex_attr_descriptions[4].binding = 0;
      vertex_attr_descriptions[4].location = 4;
      vertex_attr_descriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attr_descriptions[4].offset =
          offsetof(struct owl_model_vertex, joints0);

      vertex_attr_descriptions[5].binding = 0;
      vertex_attr_descriptions[5].location = 5;
      vertex_attr_descriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attr_descriptions[5].offset =
          offsetof(struct owl_model_vertex, weights0);
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      vertex_input_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertex_input_state.pNext = NULL;
      vertex_input_state.flags = 0;
      vertex_input_state.vertexBindingDescriptionCount = 1;
      vertex_input_state.pVertexBindingDescriptions =
          vertex_binding_descriptions;
      vertex_input_state.vertexAttributeDescriptionCount = 3;
      vertex_input_state.pVertexAttributeDescriptions =
          vertex_attr_descriptions;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      vertex_input_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      vertex_input_state.pNext = NULL;
      vertex_input_state.flags = 0;
      vertex_input_state.vertexBindingDescriptionCount = 1;
      vertex_input_state.pVertexBindingDescriptions =
          vertex_binding_descriptions;
      vertex_input_state.vertexAttributeDescriptionCount = 6;
      vertex_input_state.pVertexAttributeDescriptions =
          vertex_attr_descriptions;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      input_assembly_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      input_assembly_state.pNext = NULL;
      input_assembly_state.flags = 0;
      input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      input_assembly_state.primitiveRestartEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      viewport.x = 0.0F;
      viewport.y = 0.0F;
      viewport.width = renderer->swapchain_extent.width;
      viewport.height = renderer->swapchain_extent.height;
      viewport.minDepth = 0.0F;
      viewport.maxDepth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      scissor.extent = renderer->swapchain_extent;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
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
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      rasterization_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterization_state.pNext = NULL;
      rasterization_state.flags = 0;
      rasterization_state.depthClampEnable = VK_FALSE;
      rasterization_state.rasterizerDiscardEnable = VK_FALSE;
      rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
      rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
      rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      rasterization_state.depthBiasEnable = VK_FALSE;
      rasterization_state.depthBiasConstantFactor = 0.0F;
      rasterization_state.depthBiasClamp = 0.0F;
      rasterization_state.depthBiasSlopeFactor = 0.0F;
      rasterization_state.lineWidth = 1.0F;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      rasterization_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterization_state.pNext = NULL;
      rasterization_state.flags = 0;
      rasterization_state.depthClampEnable = VK_FALSE;
      rasterization_state.rasterizerDiscardEnable = VK_FALSE;
      rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
      rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
      rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      rasterization_state.depthBiasEnable = VK_FALSE;
      rasterization_state.depthBiasConstantFactor = 0.0F;
      rasterization_state.depthBiasClamp = 0.0F;
      rasterization_state.depthBiasSlopeFactor = 0.0F;
      rasterization_state.lineWidth = 1.0F;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
      rasterization_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterization_state.pNext = NULL;
      rasterization_state.flags = 0;
      rasterization_state.depthClampEnable = VK_FALSE;
      rasterization_state.rasterizerDiscardEnable = VK_FALSE;
      rasterization_state.polygonMode = VK_POLYGON_MODE_LINE;
      rasterization_state.cullMode = VK_CULL_MODE_BACK_BIT;
      rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
      rasterization_state.depthBiasEnable = VK_FALSE;
      rasterization_state.depthBiasConstantFactor = 0.0F;
      rasterization_state.depthBiasClamp = 0.0F;
      rasterization_state.depthBiasSlopeFactor = 0.0F;
      rasterization_state.lineWidth = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      multisample_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisample_state.pNext = NULL;
      multisample_state.flags = 0;
      multisample_state.rasterizationSamples = renderer->msaa_sample_count;
      multisample_state.sampleShadingEnable = VK_FALSE;
      multisample_state.minSampleShading = 1.0F;
      multisample_state.pSampleMask = NULL;
      multisample_state.alphaToCoverageEnable = VK_FALSE;
      multisample_state.alphaToOneEnable = VK_FALSE;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      color_blend_attachment_states[0].blendEnable = VK_FALSE;
      color_blend_attachment_states[0].srcColorBlendFactor =
          VK_BLEND_FACTOR_ONE;
      color_blend_attachment_states[0].dstColorBlendFactor =
          VK_BLEND_FACTOR_ZERO;
      color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachment_states[0].srcAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE;
      color_blend_attachment_states[0].dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ZERO;
      color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachment_states[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      color_blend_attachment_states[0].blendEnable = VK_TRUE;
      color_blend_attachment_states[0].srcColorBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachment_states[0].dstColorBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachment_states[0].colorBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachment_states[0].srcAlphaBlendFactor =
          VK_BLEND_FACTOR_SRC_ALPHA;
      color_blend_attachment_states[0].dstAlphaBlendFactor =
          VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      color_blend_attachment_states[0].alphaBlendOp = VK_BLEND_OP_ADD;
      color_blend_attachment_states[0].colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      color_blend_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      color_blend_state.pNext = NULL;
      color_blend_state.flags = 0;
      color_blend_state.logicOpEnable = VK_FALSE;
      color_blend_state.logicOp = VK_LOGIC_OP_COPY;
      color_blend_state.attachmentCount = 1;
      color_blend_state.pAttachments = color_blend_attachment_states;
      color_blend_state.blendConstants[0] = 0.0F;
      color_blend_state.blendConstants[1] = 0.0F;
      color_blend_state.blendConstants[2] = 0.0F;
      color_blend_state.blendConstants[3] = 0.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      depth_stencil_state.sType =
          VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      depth_stencil_state.pNext = NULL;
      depth_stencil_state.flags = 0;
      depth_stencil_state.depthTestEnable = VK_TRUE;
      depth_stencil_state.depthWriteEnable = VK_TRUE;
      depth_stencil_state.depthCompareOp = VK_COMPARE_OP_LESS;
      depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
      depth_stencil_state.stencilTestEnable = VK_FALSE;
      OWL_MEMSET(&depth_stencil_state.front, 0,
                 sizeof(depth_stencil_state.front));
      OWL_MEMSET(&depth_stencil_state.back, 0,
                 sizeof(depth_stencil_state.back));
      depth_stencil_state.minDepthBounds = 0.0F;
      depth_stencil_state.maxDepthBounds = 1.0F;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = renderer->basic_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = renderer->basic_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = renderer->basic_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = renderer->font_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[0].pNext = NULL;
      stages[0].flags = 0;
      stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      stages[0].module = renderer->model_vertex_shader;
      stages[0].pName = "main";
      stages[0].pSpecializationInfo = NULL;

      stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stages[1].pNext = NULL;
      stages[1].flags = 0;
      stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      stages[1].module = renderer->model_fragment_shader;
      stages[1].pName = "main";
      stages[1].pSpecializationInfo = NULL;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
      renderer->pipeline_layouts[i] = renderer->common_pipeline_layout;
      break;

    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      renderer->pipeline_layouts[i] = renderer->model_pipeline_layout;
      break;
    }

    switch (i) {
    case OWL_RENDERER_PIPELINE_TYPE_MAIN:
    case OWL_RENDERER_PIPELINE_TYPE_WIRES:
    case OWL_RENDERER_PIPELINE_TYPE_FONT:
    case OWL_RENDERER_PIPELINE_TYPE_MODEL:
      pipeline_create_info.sType =
          VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
      pipeline_create_info.pNext = NULL;
      pipeline_create_info.flags = 0;
      pipeline_create_info.stageCount = OWL_ARRAY_SIZE(stages);
      pipeline_create_info.pStages = stages;
      pipeline_create_info.pVertexInputState = &vertex_input_state;
      pipeline_create_info.pInputAssemblyState = &input_assembly_state;
      pipeline_create_info.pTessellationState = NULL;
      pipeline_create_info.pViewportState = &viewport_state;
      pipeline_create_info.pRasterizationState = &rasterization_state;
      pipeline_create_info.pMultisampleState = &multisample_state;
      pipeline_create_info.pDepthStencilState = &depth_stencil_state;
      pipeline_create_info.pColorBlendState = &color_blend_state;
      pipeline_create_info.pDynamicState = NULL;
      pipeline_create_info.layout = renderer->pipeline_layouts[i];
      pipeline_create_info.renderPass = renderer->main_render_pass;
      pipeline_create_info.subpass = 0;
      pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
      pipeline_create_info.basePipelineIndex = -1;
      break;
    }

    OWL_VK_CHECK(vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                           &pipeline_create_info, NULL,
                                           &renderer->pipelines[i]));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_pipelines_(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_RENDERER_PIPELINE_TYPE_MODEL],
                    NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_RENDERER_PIPELINE_TYPE_FONT], NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_RENDERER_PIPELINE_TYPE_WIRES],
                    NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_RENDERER_PIPELINE_TYPE_MAIN], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_frame_commands_(struct owl_renderer *renderer) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  renderer->active_frame_index = 0;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
    VkCommandPoolCreateInfo pool_create_info;

    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.pNext = NULL;
    pool_create_info.flags = 0;
    pool_create_info.queueFamilyIndex = renderer->graphics_queue_family_index;

    OWL_VK_CHECK(vkCreateCommandPool(renderer->device, &pool_create_info, NULL,
                                     &renderer->frame_command_pools[i]));
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
    VkCommandBufferAllocateInfo command_buffer_allocate_info;

    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = renderer->frame_command_pools[i];
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    OWL_VK_CHECK(vkAllocateCommandBuffers(renderer->device,
                                          &command_buffer_allocate_info,
                                          &renderer->frame_command_buffers[i]));
  }

  renderer->active_frame_command_buffer =
      renderer->frame_command_buffers[renderer->active_frame_index];
  renderer->active_frame_command_pool =
      renderer->frame_command_pools[renderer->active_frame_index];

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_frame_commands_(struct owl_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkFreeCommandBuffers(renderer->device, renderer->frame_command_pools[i], 1,
                         &renderer->frame_command_buffers[i]);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkDestroyCommandPool(renderer->device, renderer->frame_command_pools[i],
                         NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_frame_sync_(struct owl_renderer *renderer) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
    VkFenceCreateInfo fence_create_info;
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = NULL;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    OWL_VK_CHECK(vkCreateFence(renderer->device, &fence_create_info, NULL,
                               &renderer->in_flight_fences[i]));
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore_create_info;
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = NULL;
    semaphore_create_info.flags = 0;
    OWL_VK_CHECK(vkCreateSemaphore(renderer->device, &semaphore_create_info,
                                   NULL, &renderer->render_done_semaphores[i]));
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore_create_info;
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = NULL;
    semaphore_create_info.flags = 0;
    OWL_VK_CHECK(vkCreateSemaphore(renderer->device, &semaphore_create_info,
                                   NULL,
                                   &renderer->image_available_semaphores[i]));
  }

  renderer->active_in_flight_fence =
      renderer->in_flight_fences[renderer->active_frame_index];
  renderer->active_render_done_semaphore =
      renderer->render_done_semaphores[renderer->active_frame_index];
  renderer->active_image_available_semaphore =
      renderer->image_available_semaphores[renderer->active_frame_index];

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_frame_sync_(struct owl_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkDestroyFence(renderer->device, renderer->in_flight_fences[i], NULL);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkDestroySemaphore(renderer->device, renderer->render_done_semaphores[i],
                       NULL);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkDestroySemaphore(renderer->device,
                       renderer->image_available_semaphores[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_garbage_(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  renderer->garbage_buffers_count = 0;
  renderer->garbage_memories_count = 0;
  renderer->garbage_common_ubo_sets_count = 0;
  renderer->garbage_model_ubo_sets_count = 0;
  renderer->garbage_model_ubo_params_sets_count = 0;

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_garbage_(struct owl_renderer *renderer) {
  owl_i32 i;

  for (i = 0; i < renderer->garbage_model_ubo_params_sets_count; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->common_set_pool, 1,
                         &renderer->garbage_model_ubo_params_sets[i]);

  renderer->garbage_model_ubo_params_sets_count = 0;

  for (i = 0; i < renderer->garbage_model_ubo_sets_count; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->common_set_pool, 1,
                         &renderer->garbage_model_ubo_sets[i]);

  renderer->garbage_model_ubo_sets_count = 0;

  for (i = 0; i < renderer->garbage_common_ubo_sets_count; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->common_set_pool, 1,
                         &renderer->garbage_common_ubo_sets[i]);

  renderer->garbage_common_ubo_sets_count = 0;

  for (i = 0; i < renderer->garbage_memories_count; ++i)
    vkFreeMemory(renderer->device, renderer->garbage_memories[i], NULL);

  renderer->garbage_memories_count = 0;

  for (i = 0; i < renderer->garbage_buffers_count; ++i)
    vkDestroyBuffer(renderer->device, renderer->garbage_buffers[i], NULL);

  renderer->garbage_buffers_count = 0;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_dynamic_heap_(struct owl_renderer *renderer, owl_u64 size) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  renderer->dynamic_heap_offset = 0;
  renderer->dynamic_heap_buffer_size = size;

  /* init buffers */
  {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      OWL_VK_CHECK(vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                                  &renderer->dynamic_heap_buffers[i]));
  }

  /* init memory */
  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetBufferMemoryRequirements(
        renderer->device, renderer->dynamic_heap_buffers[0], &requirements);

    renderer->dynamic_heap_buffer_alignment = requirements.alignment;
    renderer->dynamic_heap_buffer_aligned_size =
        OWL_ALIGNU2(size, requirements.alignment);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize =
        renderer->dynamic_heap_buffer_aligned_size *
        OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

    OWL_VK_CHECK(vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                  &renderer->dynamic_heap_memory));
  }

  /* bind buffers to memory */
  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      OWL_VK_CHECK(vkBindBufferMemory(
          renderer->device, renderer->dynamic_heap_buffers[i],
          renderer->dynamic_heap_memory,
          i * renderer->dynamic_heap_buffer_aligned_size));
  }

  /* map memory */
  {
    void *data;
    OWL_VK_CHECK(vkMapMemory(renderer->device, renderer->dynamic_heap_memory, 0,
                             VK_WHOLE_SIZE, 0, &data));

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      renderer->dynamic_heap_datas[i] =
          &((owl_byte *)data)[i * renderer->dynamic_heap_buffer_aligned_size];
  }

  {
    VkDescriptorSetAllocateInfo set_allocate_info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      layouts[i] = renderer->vertex_ubo_set_layout;

    set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate_info.pNext = NULL;
    set_allocate_info.descriptorPool = renderer->common_set_pool;
    set_allocate_info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
    set_allocate_info.pSetLayouts = layouts;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set_allocate_info,
                                 renderer->dynamic_heap_common_ubo_sets));
  }

  {
    VkDescriptorSetAllocateInfo set_allocate_info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      layouts[i] = renderer->shared_ubo_set_layout;

    set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate_info.pNext = NULL;
    set_allocate_info.descriptorPool = renderer->common_set_pool;
    set_allocate_info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
    set_allocate_info.pSetLayouts = layouts;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set_allocate_info,
                                 renderer->dynamic_heap_model_ubo_sets));
  }

  {
    VkDescriptorSetAllocateInfo set_allocate_info;
    VkDescriptorSetLayout layouts[OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT];

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      layouts[i] = renderer->fragment_ubo_set_layout;

    set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate_info.pNext = NULL;
    set_allocate_info.descriptorPool = renderer->common_set_pool;
    set_allocate_info.descriptorSetCount = OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;
    set_allocate_info.pSetLayouts = layouts;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set_allocate_info,
                                 renderer->dynamic_heap_model_ubo_params_sets));
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buffer;

      buffer.buffer = renderer->dynamic_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_draw_command_uniform);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = renderer->dynamic_heap_common_ubo_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = renderer->dynamic_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_model_uniform);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = renderer->dynamic_heap_model_ubo_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = renderer->dynamic_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_model_uniform_params);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = renderer->dynamic_heap_model_ubo_params_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buffer;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
  }

  renderer->active_dynamic_heap_data =
      renderer->dynamic_heap_datas[renderer->active_frame_index];
  renderer->active_dynamic_heap_buffer =
      renderer->dynamic_heap_buffers[renderer->active_frame_index];
  renderer->active_dynamic_heap_common_ubo_set =
      renderer->dynamic_heap_common_ubo_sets[renderer->active_frame_index];
  renderer->active_dynamic_heap_model_ubo_set =
      renderer->dynamic_heap_model_ubo_sets[renderer->active_frame_index];
  renderer->active_dynamic_heap_model_ubo_params_set =
      renderer
          ->dynamic_heap_model_ubo_params_sets[renderer->active_frame_index];

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_dynamic_heap_(struct owl_renderer *renderer) {
  owl_i32 i;

  vkFreeDescriptorSets(renderer->device, renderer->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                       renderer->dynamic_heap_model_ubo_params_sets);

  vkFreeDescriptorSets(renderer->device, renderer->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                       renderer->dynamic_heap_model_ubo_sets);

  vkFreeDescriptorSets(renderer->device, renderer->common_set_pool,
                       OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT,
                       renderer->dynamic_heap_common_ubo_sets);

  vkFreeMemory(renderer->device, renderer->dynamic_heap_memory, NULL);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
    vkDestroyBuffer(renderer->device, renderer->dynamic_heap_buffers[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_move_dynamic_heap_to_garbage_(struct owl_renderer *renderer) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  {
    owl_i32 previous_count = renderer->garbage_buffers_count;
    owl_i32 new_count = previous_count + OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      renderer->garbage_buffers[previous_count + i] =
          renderer->dynamic_heap_buffers[i];

    renderer->garbage_buffers_count = new_count;
  }

  {
    owl_i32 previous_count = renderer->garbage_common_ubo_sets_count;
    owl_i32 new_count = previous_count + OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      renderer->garbage_common_ubo_sets[previous_count + i] =
          renderer->dynamic_heap_common_ubo_sets[i];

    renderer->garbage_common_ubo_sets_count = new_count;
  }

  {
    owl_i32 previous_count = renderer->garbage_model_ubo_sets_count;
    owl_i32 new_count = previous_count + OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      renderer->garbage_model_ubo_sets[previous_count + i] =
          renderer->dynamic_heap_model_ubo_sets[i];

    renderer->garbage_model_ubo_sets_count = new_count;
  }

  {
    owl_i32 previous_count = renderer->garbage_model_ubo_params_sets_count;
    owl_i32 new_count = previous_count + OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT; ++i)
      renderer->garbage_model_ubo_params_sets[previous_count + i] =
          renderer->dynamic_heap_model_ubo_params_sets[i];

    renderer->garbage_model_ubo_sets_count = new_count;
  }

  {
    owl_i32 previous_count = renderer->garbage_memories_count;
    owl_i32 new_count = previous_count + 1;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS_COUNT <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    renderer->garbage_memories[previous_count + 0] =
        renderer->dynamic_heap_memory;

    renderer->garbage_memories_count = new_count;
  }

  renderer->active_dynamic_heap_data = NULL;
  renderer->active_dynamic_heap_buffer = VK_NULL_HANDLE;
  renderer->active_dynamic_heap_common_ubo_set = VK_NULL_HANDLE;
  renderer->active_dynamic_heap_model_ubo_set = VK_NULL_HANDLE;
  renderer->active_dynamic_heap_model_ubo_params_set = VK_NULL_HANDLE;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_init_image_manager_(struct owl_renderer *renderer) {
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT; ++i)
    renderer->image_manager_slots[i] = 0;

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_image_manager_(struct owl_renderer *renderer) {
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT; ++i) {
    if (!renderer->image_manager_slots[i])
      continue;

    OWL_DEBUG_LOG("(owl_renderer_image: %i) ayo? free your resources man\n", i);

    renderer->image_manager_slots[i] = 0;

    vkFreeDescriptorSets(renderer->device, renderer->common_set_pool, 1,
                         &renderer->image_manager_sets[i]);
    vkDestroySampler(renderer->device, renderer->image_manager_samplers[i],
                     NULL);
    vkDestroyImageView(renderer->device, renderer->image_manager_views[i],
                       NULL);
    vkFreeMemory(renderer->device, renderer->image_manager_memories[i], NULL);
    vkDestroyImage(renderer->device, renderer->image_manager_images[i], NULL);
  }
}

enum owl_code owl_renderer_init(struct owl_renderer_init_desc const *desc,
                                struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  renderer->window_width = desc->window_width;
  renderer->window_height = desc->window_height;
  renderer->framebuffer_width = desc->framebuffer_width;
  renderer->framebuffer_height = desc->framebuffer_height;
  renderer->framebuffer_ratio =
      (float)desc->framebuffer_width / (float)desc->framebuffer_height;
  renderer->immidiate_command_buffer = VK_NULL_HANDLE;

  if (OWL_SUCCESS != (code = owl_renderer_init_instance_(desc, renderer)))
    goto end;

#if defined(OWL_ENABLE_VALIDATION)
  if (OWL_SUCCESS != (code = owl_renderer_init_debug_(renderer)))
    goto end_err_deinit_instance;

  if (OWL_SUCCESS != (code = owl_renderer_init_surface_(desc, renderer)))
    goto end_err_deinit_debug;
#else  /* OWL_ENABLE_VALIDATION */
  if (OWL_SUCCESS != (code = owl_renderer_init_surface_(desc, renderer)))
    goto end_err_deinit_instance;
#endif /* OWL_ENABLE_VALIDATION */

  if (OWL_SUCCESS != (code = owl_renderer_init_device_(renderer)))
    goto end_err_deinit_surface;

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_(desc, renderer)))
    goto end_err_deinit_device;

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_views_(renderer)))
    goto end_err_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_renderer_init_common_pools_(renderer)))
    goto end_err_deinit_swapchain_views;

  if (OWL_SUCCESS != (code = owl_renderer_init_main_render_pass_(renderer)))
    goto end_err_deinit_common_pools;

  if (OWL_SUCCESS != (code = owl_renderer_init_attachments_(renderer)))
    goto end_err_deinit_main_render_pass;

  if (OWL_SUCCESS !=
      (code = owl_renderer_init_swapchain_framebuffers_(renderer)))
    goto end_err_deinit_attachments;

  if (OWL_SUCCESS != (code = owl_renderer_init_set_layouts_(renderer)))
    goto end_err_deinit_swapchain_framebuffers;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipeline_layouts_(renderer)))
    goto end_err_deinit_set_layouts;

  if (OWL_SUCCESS != (code = owl_renderer_init_shaders_(renderer)))
    goto end_err_deinit_pipeline_layouts;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipelines_(renderer)))
    goto end_err_deinit_shaders;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_commands_(renderer)))
    goto end_err_deinit_pipelines;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_sync_(renderer)))
    goto end_err_deinit_frame_commands;

  if (OWL_SUCCESS != (code = owl_renderer_init_garbage_(renderer)))
    goto end_err_deinit_frame_sync;

  if (OWL_SUCCESS !=
      (code = owl_renderer_init_dynamic_heap_(renderer, 1048576)))
    goto end_err_deinit_garbage;

  if (OWL_SUCCESS != (code = owl_renderer_init_image_manager_(renderer)))
    goto end_err_deinit_dynamic_heap;

  goto end;

end_err_deinit_dynamic_heap:
  owl_renderer_deinit_dynamic_heap_(renderer);

end_err_deinit_garbage:
  owl_renderer_deinit_garbage_(renderer);

end_err_deinit_frame_sync:
  owl_renderer_deinit_frame_sync_(renderer);

end_err_deinit_frame_commands:
  owl_renderer_deinit_frame_commands_(renderer);

end_err_deinit_pipelines:
  owl_renderer_deinit_pipelines_(renderer);

end_err_deinit_shaders:
  owl_renderer_deinit_shaders_(renderer);

end_err_deinit_pipeline_layouts:
  owl_renderer_deinit_pipeline_layouts_(renderer);

end_err_deinit_set_layouts:
  owl_renderer_deinit_set_layouts_(renderer);

end_err_deinit_swapchain_framebuffers:
  owl_renderer_deinit_swapchain_framebuffers_(renderer);

end_err_deinit_attachments:
  owl_renderer_deinit_attachments_(renderer);

end_err_deinit_main_render_pass:
  owl_renderer_deinit_main_render_pass_(renderer);

end_err_deinit_common_pools:
  owl_renderer_deinit_common_pools_(renderer);

end_err_deinit_swapchain_views:
  owl_renderer_deinit_swapchain_views_(renderer);

end_err_deinit_swapchain:
  owl_renderer_deinit_swapchain_(renderer);

end_err_deinit_device:
  owl_renderer_deinit_device_(renderer);

end_err_deinit_surface:
  owl_renderer_deinit_surface_(renderer);

#if defined(OWL_ENABLE_VALIDATION)
end_err_deinit_debug:
  owl_renderer_deinit_debug_(renderer);
#endif /* OWL_ENABLE_VALIDATION */

end_err_deinit_instance:
  owl_renderer_deinit_instance_(renderer);

end:
  return code;
}

void owl_renderer_deinit(struct owl_renderer *renderer) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  owl_renderer_deinit_image_manager_(renderer);
  owl_renderer_deinit_dynamic_heap_(renderer);
  owl_renderer_deinit_garbage_(renderer);
  owl_renderer_deinit_frame_sync_(renderer);
  owl_renderer_deinit_frame_commands_(renderer);
  owl_renderer_deinit_pipelines_(renderer);
  owl_renderer_deinit_shaders_(renderer);
  owl_renderer_deinit_pipeline_layouts_(renderer);
  owl_renderer_deinit_set_layouts_(renderer);
  owl_renderer_deinit_swapchain_framebuffers_(renderer);
  owl_renderer_deinit_attachments_(renderer);
  owl_renderer_deinit_main_render_pass_(renderer);
  owl_renderer_deinit_common_pools_(renderer);
  owl_renderer_deinit_swapchain_views_(renderer);
  owl_renderer_deinit_swapchain_(renderer);
  owl_renderer_deinit_device_(renderer);
  owl_renderer_deinit_surface_(renderer);

#if defined(OWL_ENABLE_VALIDATION)
  owl_renderer_deinit_debug_(renderer);
#endif /* OWL_ENABLE_VALIDATION */

  owl_renderer_deinit_instance_(renderer);
}

OWL_INTERNAL enum owl_code
owl_renderer_reserve_dynamic_heap_memory_(struct owl_renderer *renderer,
                                          owl_u64 size) {
  enum owl_code code = OWL_SUCCESS;
  owl_u64 required = renderer->dynamic_heap_offset + size;

  if (required < renderer->dynamic_heap_buffer_size)
    goto end;

  vkUnmapMemory(renderer->device, renderer->dynamic_heap_memory);

  code = owl_renderer_move_dynamic_heap_to_garbage_(renderer);

  if (OWL_SUCCESS != code)
    goto end;

  code = owl_renderer_init_dynamic_heap_(renderer, 2 * required);

  if (OWL_SUCCESS != code)
    goto end;

end:
  return code;
}

enum owl_code
owl_renderer_resize_swapchain(struct owl_renderer_init_desc const *desc,
                              struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  renderer->window_width = desc->window_width;
  renderer->window_height = desc->window_height;
  renderer->framebuffer_width = desc->framebuffer_width;
  renderer->framebuffer_height = desc->framebuffer_height;
  renderer->framebuffer_ratio =
      (float)desc->framebuffer_width / (float)desc->framebuffer_height;

  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  owl_renderer_deinit_pipelines_(renderer);
  owl_renderer_deinit_swapchain_framebuffers_(renderer);
  owl_renderer_deinit_attachments_(renderer);
  owl_renderer_deinit_main_render_pass_(renderer);
  owl_renderer_deinit_frame_sync_(renderer);
  owl_renderer_deinit_swapchain_views_(renderer);
  owl_renderer_deinit_swapchain_(renderer);

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_(desc, renderer)))
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_views_(renderer)))
    goto end_err_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_sync_(renderer)))
    goto end_err_deinit_swapchain_views;

  if (OWL_SUCCESS != (code = owl_renderer_init_main_render_pass_(renderer)))
    goto end_err_deinit_frame_sync;

  if (OWL_SUCCESS != (code = owl_renderer_init_attachments_(renderer)))
    goto end_err_deinit_main_render_pass;

  if (OWL_SUCCESS !=
      (code = owl_renderer_init_swapchain_framebuffers_(renderer)))
    goto end_err_deinit_main_attachments;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipelines_(renderer)))
    goto end_err_deinit_main_framebuffers;

  goto end;

end_err_deinit_main_framebuffers:
  owl_renderer_deinit_swapchain_framebuffers_(renderer);

end_err_deinit_main_attachments:
  owl_renderer_deinit_attachments_(renderer);

end_err_deinit_main_render_pass:
  owl_renderer_deinit_main_render_pass_(renderer);

end_err_deinit_frame_sync:
  owl_renderer_deinit_frame_sync_(renderer);

end_err_deinit_swapchain_views:
  owl_renderer_deinit_swapchain_views_(renderer);

end_err_deinit_swapchain:
  owl_renderer_deinit_swapchain_(renderer);

end:
  return code;
}

owl_b32
owl_renderer_is_dynamic_heap_offset_clear(struct owl_renderer const *renderer) {
  return 0 == renderer->dynamic_heap_offset;
}

OWL_INTERNAL void owl_renderer_clear_garbage_(struct owl_renderer *renderer) {
  owl_renderer_deinit_garbage_(renderer);
}

void owl_renderer_clear_dynamic_heap_offset(struct owl_renderer *renderer) {
  renderer->dynamic_heap_offset = 0;
}

void *owl_renderer_dynamic_heap_alloc(
    struct owl_renderer *renderer, owl_u64 size,
    struct owl_renderer_dynamic_heap_reference *reference) {
  owl_byte *data = NULL;

  if (OWL_SUCCESS != owl_renderer_reserve_dynamic_heap_memory_(renderer, size))
    goto end;

  reference->offset32 = (owl_u32)renderer->dynamic_heap_offset;
  reference->offset = renderer->dynamic_heap_offset;
  reference->buffer = renderer->active_dynamic_heap_buffer;
  reference->common_ubo_set = renderer->active_dynamic_heap_common_ubo_set;
  reference->model_ubo_set = renderer->active_dynamic_heap_model_ubo_set;
  reference->model_ubo_params_set =
      renderer->active_dynamic_heap_model_ubo_params_set;

  data = &renderer->active_dynamic_heap_data[renderer->dynamic_heap_offset];

  renderer->dynamic_heap_offset =
      OWL_ALIGNU2(renderer->dynamic_heap_offset + size,
                  renderer->dynamic_heap_buffer_alignment);

end:
  return data;
}

enum owl_code owl_renderer_dynamic_heap_submit(
    struct owl_renderer *renderer, owl_u64 size, void const *src,
    struct owl_renderer_dynamic_heap_reference *reference) {

  owl_byte *data;
  enum owl_code code = OWL_SUCCESS;

  if (!(data = owl_renderer_dynamic_heap_alloc(renderer, size, reference))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  OWL_MEMCPY(data, src, size);

end:
  return code;
}

owl_u32
owl_renderer_find_memory_type(struct owl_renderer const *renderer,
                              VkMemoryRequirements const *requirements,
                              enum owl_renderer_memory_visibility visibility) {
  owl_u32 i;
  VkMemoryPropertyFlags property;
  owl_u32 filter = requirements->memoryTypeBits;

  switch (visibility) {
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU:
    property = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU_CACHED:
    property = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT:
    property = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case OWL_RENDERER_MEMORY_VISIBILITY_GPU:
    property = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  }

  for (i = 0; i < renderer->device_memory_properties.memoryTypeCount; ++i) {
    owl_u32 f = renderer->device_memory_properties.memoryTypes[i].propertyFlags;

    if ((f & property) && (filter & (1U << i)))
      return i;
  }

  return OWL_RENDERER_MEMORY_TYPE_NONE;
}

enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *renderer,
                                         enum owl_renderer_pipeline_type type) {
  enum owl_code code = OWL_SUCCESS;

  if (OWL_RENDERER_PIPELINE_TYPE_NONE == type)
    goto end;

  renderer->active_pipeline = renderer->pipelines[type];
  renderer->active_pipeline_layout = renderer->pipeline_layouts[type];

  vkCmdBindPipeline(renderer->active_frame_command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->active_pipeline);

end:
  return code;
}
enum owl_code
owl_renderer_begin_immidiate_command_buffer(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(!renderer->immidiate_command_buffer);

  {
    VkCommandBufferAllocateInfo command_buffer_allocate_info;

    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = renderer->transient_command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    OWL_VK_CHECK(vkAllocateCommandBuffers(renderer->device,
                                          &command_buffer_allocate_info,
                                          &renderer->immidiate_command_buffer));
  }

  {
    VkCommandBufferBeginInfo begin_info;

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = NULL;

    OWL_VK_CHECK(
        vkBeginCommandBuffer(renderer->immidiate_command_buffer, &begin_info));
  }

  return code;
}

enum owl_code
owl_renderer_end_immidiate_command_buffer(struct owl_renderer *renderer) {
  VkSubmitInfo submit;
  enum owl_code code = OWL_SUCCESS;

  OWL_ASSERT(renderer->immidiate_command_buffer);
  OWL_VK_CHECK(vkEndCommandBuffer(renderer->immidiate_command_buffer));

  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = NULL;
  submit.waitSemaphoreCount = 0;
  submit.pWaitSemaphores = NULL;
  submit.pWaitDstStageMask = NULL;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &renderer->immidiate_command_buffer;
  submit.signalSemaphoreCount = 0;
  submit.pSignalSemaphores = NULL;

  OWL_VK_CHECK(vkQueueSubmit(renderer->graphics_queue, 1, &submit, NULL));
  OWL_VK_CHECK(vkQueueWaitIdle(renderer->graphics_queue));

  vkFreeCommandBuffers(renderer->device, renderer->transient_command_pool, 1,
                       &renderer->immidiate_command_buffer);

  renderer->immidiate_command_buffer = VK_NULL_HANDLE;

  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_acquire_next_image_(struct owl_renderer *renderer) {
  VkResult result;
  enum owl_code code = OWL_SUCCESS;

  result = vkAcquireNextImageKHR(
      renderer->device, renderer->swapchain, OWL_ACQUIRE_IMAGE_TIMEOUT,
      renderer->active_image_available_semaphore, VK_NULL_HANDLE,
      &renderer->active_swapchain_image_index);

  if (VK_ERROR_OUT_OF_DATE_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  else if (VK_SUBOPTIMAL_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  else if (VK_ERROR_SURFACE_LOST_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;

  renderer->active_swapchain_image =
      renderer->swapchain_images[renderer->active_swapchain_image_index];

  renderer->active_swapchain_framebuffer =
      renderer->swapchain_framebuffers[renderer->active_swapchain_image_index];

  return code;
}

OWL_INTERNAL void owl_renderer_prepare_frame_(struct owl_renderer *renderer) {
  OWL_VK_CHECK(vkWaitForFences(renderer->device, 1,
                               &renderer->active_in_flight_fence, VK_TRUE,
                               OWL_WAIT_FOR_FENCES_TIMEOUT));
  OWL_VK_CHECK(
      vkResetFences(renderer->device, 1, &renderer->active_in_flight_fence));
  OWL_VK_CHECK(vkResetCommandPool(renderer->device,
                                  renderer->active_frame_command_pool, 0));
}

OWL_INTERNAL void owl_renderer_begin_recording_(struct owl_renderer *renderer) {
  {
    VkCommandBufferBeginInfo begin_info;

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = NULL;

    OWL_VK_CHECK(vkBeginCommandBuffer(renderer->active_frame_command_buffer,
                                      &begin_info));
  }

  {
    VkRenderPassBeginInfo begin_info;

    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.renderPass = renderer->main_render_pass;
    begin_info.framebuffer = renderer->active_swapchain_framebuffer;
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent = renderer->swapchain_extent;
    begin_info.clearValueCount = OWL_RENDERER_CLEAR_VALUES_COUNT;
    begin_info.pClearValues = renderer->swapchain_clear_values;

    vkCmdBeginRenderPass(renderer->active_frame_command_buffer, &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
  }
}

enum owl_code owl_renderer_begin_frame(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_acquire_next_image_(renderer)))
    goto end;

  owl_renderer_prepare_frame_(renderer);
  owl_renderer_begin_recording_(renderer);

end:
  return code;
}

OWL_INTERNAL void owl_renderer_end_recording_(struct owl_renderer *renderer) {
  vkCmdEndRenderPass(renderer->active_frame_command_buffer);
  OWL_VK_CHECK(vkEndCommandBuffer(renderer->active_frame_command_buffer));
}

OWL_INTERNAL void owl_renderer_submit_graphics_(struct owl_renderer *renderer) {
  VkSubmitInfo submit;
  VkPipelineStageFlags const stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = NULL;
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &renderer->active_image_available_semaphore;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &renderer->active_render_done_semaphore;
  submit.pWaitDstStageMask = &stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &renderer->active_frame_command_buffer;

  OWL_VK_CHECK(vkQueueSubmit(renderer->graphics_queue, 1, &submit,
                             renderer->active_in_flight_fence));
}

OWL_INTERNAL enum owl_code
owl_renderer_present_swapchain_(struct owl_renderer *renderer) {
  VkResult result;
  VkPresentInfoKHR present;
  enum owl_code code = OWL_SUCCESS;

  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = NULL;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &renderer->active_render_done_semaphore;
  present.swapchainCount = 1;
  present.pSwapchains = &renderer->swapchain;
  present.pImageIndices = &renderer->active_swapchain_image_index;
  present.pResults = NULL;

  result = vkQueuePresentKHR(renderer->present_queue, &present);

  if (VK_ERROR_OUT_OF_DATE_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  else if (VK_SUBOPTIMAL_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  else if (VK_ERROR_SURFACE_LOST_KHR == result)
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;

  return code;
}

OWL_INTERNAL void
owl_renderer_update_frame_actives_(struct owl_renderer *renderer) {
  if (OWL_RENDERER_IN_FLIGHT_FRAMES_COUNT == ++renderer->active_frame_index)
    renderer->active_frame_index = 0;

  renderer->active_in_flight_fence =
      renderer->in_flight_fences[renderer->active_frame_index];
  renderer->active_image_available_semaphore =
      renderer->image_available_semaphores[renderer->active_frame_index];
  renderer->active_render_done_semaphore =
      renderer->render_done_semaphores[renderer->active_frame_index];
  renderer->active_frame_command_buffer =
      renderer->frame_command_buffers[renderer->active_frame_index];
  renderer->active_frame_command_pool =
      renderer->frame_command_pools[renderer->active_frame_index];
  renderer->active_dynamic_heap_data =
      renderer->dynamic_heap_datas[renderer->active_frame_index];
  renderer->active_dynamic_heap_buffer =
      renderer->dynamic_heap_buffers[renderer->active_frame_index];
  renderer->active_dynamic_heap_common_ubo_set =
      renderer->dynamic_heap_common_ubo_sets[renderer->active_frame_index];
  renderer->active_dynamic_heap_model_ubo_set =
      renderer->dynamic_heap_model_ubo_sets[renderer->active_frame_index];
  renderer->active_dynamic_heap_model_ubo_params_set =
      renderer
          ->dynamic_heap_model_ubo_params_sets[renderer->active_frame_index];
}

enum owl_code owl_renderer_end_frame(struct owl_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  owl_renderer_end_recording_(renderer);
  owl_renderer_submit_graphics_(renderer);

  if (OWL_SUCCESS != (code = owl_renderer_present_swapchain_(renderer)))
    goto end;

  owl_renderer_update_frame_actives_(renderer);
  owl_renderer_clear_dynamic_heap_offset(renderer);
  owl_renderer_clear_garbage_(renderer);

end:
  return code;
}

OWL_INTERNAL VkFormat
owl_renderer_pixel_format_as_vk_format(enum owl_renderer_pixel_format format) {
  switch (format) {
  case OWL_RENDERER_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

OWL_INTERNAL VkFilter owl_renderer_sampler_filter_as_vk_filter_(
    enum owl_renderer_sampler_filter filter) {
  switch (filter) {
  case OWL_RENDERER_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_RENDERER_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

OWL_INTERNAL VkSamplerMipmapMode
owl_renderer_sampler_mip_mode_as_vk_sampler_mipmap_mode_(
    enum owl_renderer_sampler_mip_mode mode) {
  switch (mode) {
  case OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

OWL_INTERNAL VkSamplerAddressMode
owl_renderer_sampler_addr_mode_as_vk_sampler_address_mode_(
    enum owl_renderer_sampler_addr_mode mode) {
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
owl_renderer_image_manager_find_slot_(struct owl_renderer *renderer,
                                      struct owl_renderer_image *image) {
  owl_i32 j;
  enum owl_code code = OWL_SUCCESS;

  for (j = 0; j < OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT; ++j) {
    if (!renderer->image_manager_slots[j]) {
      image->slot = j;
      renderer->image_manager_slots[j] = 1;
      goto end;
    }
  }

  code = OWL_ERROR_UNKNOWN;
  image->slot = OWL_RENDERER_IMAGE_MANAGER_SLOTS_COUNT;

end:
  return code;
}

OWL_INTERNAL owl_u32 owl_calculate_mip_levels_(owl_u32 width, owl_u32 height) {
  return (owl_u32)(floor(log2(OWL_MAX(width, height))) + 1);
}

struct owl_renderer_image_transition_desc {
  owl_u32 mips;
  owl_u32 layers;
  VkImageLayout src_layout;
  VkImageLayout dst_layout;
};

struct owl_renderer_image_generate_mips_desc {
  owl_i32 width;
  owl_i32 height;
  owl_u32 mips;
};

OWL_INTERNAL enum owl_code owl_renderer_image_transition_(
    struct owl_renderer const *renderer, struct owl_renderer_image const *image,
    struct owl_renderer_image_transition_desc const *transition_desc) {
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;
  enum owl_code code = OWL_SUCCESS;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = transition_desc->src_layout;
  barrier.newLayout = transition_desc->dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = renderer->image_manager_images[image->slot];
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = transition_desc->mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = transition_desc->layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == transition_desc->src_layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == transition_desc->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ==
                 transition_desc->src_layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ==
                 transition_desc->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == transition_desc->src_layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ==
                 transition_desc->dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ==
      transition_desc->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ==
             transition_desc->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ==
             transition_desc->dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    OWL_ASSERT(0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

  vkCmdPipelineBarrier(renderer->immidiate_command_buffer, src, dst, 0, 0, NULL,
                       0, NULL, 1, &barrier);

end:
  return code;
}

OWL_INTERNAL enum owl_code owl_renderer_image_generate_mips_(
    struct owl_renderer const *renderer, struct owl_renderer_image const *image,
    struct owl_renderer_image_generate_mips_desc const *mips_desc) {
  owl_u32 i;
  owl_i32 width;
  owl_i32 height;
  VkImageMemoryBarrier barrier;
  enum owl_code code = OWL_SUCCESS;

  if (1 == mips_desc->mips || 0 == mips_desc->mips)
    goto end;

  width = mips_desc->width;
  height = mips_desc->height;

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = renderer->image_manager_images[image->slot];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (mips_desc->mips - 1); ++i) {
    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier(
        renderer->immidiate_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
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

      vkCmdBlitImage(renderer->immidiate_command_buffer,
                     renderer->image_manager_images[image->slot],
                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     renderer->image_manager_images[image->slot],
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                     VK_FILTER_LINEAR);
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(renderer->immidiate_command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                         NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = mips_desc->mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(
      renderer->immidiate_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

end:
  return code;
}

struct owl_renderer_image_load_state {
  owl_u32 width;
  owl_u32 height;
  owl_u32 mips;
  enum owl_renderer_pixel_format format;
  struct owl_renderer_dynamic_heap_reference reference;
};

enum owl_code owl_renderer_init_image_load_state(
    struct owl_renderer *renderer,
    struct owl_renderer_image_init_desc const *desc,
    struct owl_renderer_image_load_state *state) {
  enum owl_code code = OWL_SUCCESS;

  switch (desc->source_type) {
  case OWL_RENDERER_IMAGE_SOURCE_TYPE_FILE: {
    owl_i32 source_width;
    owl_i32 source_height;
    owl_i32 source_channels;
    owl_u64 size;
    owl_byte *data;

    data = stbi_load(desc->source_path, &source_width, &source_height,
                     &source_channels, STBI_rgb_alpha);
    if (!data) {
      code = OWL_ERROR_UNKNOWN;
      goto end;
    }

    state->format = OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB;
    state->width = (owl_u32)source_width;
    state->height = (owl_u32)source_height;
    size = owl_renderer_pixel_format_size(state->format) * state->width *
           state->height;
    code = owl_renderer_dynamic_heap_submit(renderer, size, data,
                                            &state->reference);

    stbi_image_free(data);

    if (OWL_SUCCESS != code)
      goto end;

  } break;
  case OWL_RENDERER_IMAGE_SOURCE_TYPE_DATA: {
    owl_u64 size;

    state->format = desc->source_data_format;
    state->width = (owl_u32)desc->source_data_width;
    state->height = (owl_u32)desc->source_data_height;
    size = owl_renderer_pixel_format_size(state->format) * state->width *
           state->height;
    code = owl_renderer_dynamic_heap_submit(renderer, size, desc->source_data,
                                            &state->reference);

    if (OWL_SUCCESS != code)
      goto end;

  } break;
  }

  state->mips = owl_calculate_mip_levels_(state->width, state->height);

end:
  return code;
}
enum owl_code
owl_renderer_init_image(struct owl_renderer *renderer,
                        struct owl_renderer_image_init_desc const *desc,
                        struct owl_renderer_image *image) {
  enum owl_code code = OWL_SUCCESS;
  struct owl_renderer_image_load_state state;

  OWL_ASSERT(owl_renderer_is_dynamic_heap_offset_clear(renderer));

  if (OWL_SUCCESS != code)
    goto end;

  code = owl_renderer_init_image_load_state(renderer, desc, &state);

  if (OWL_SUCCESS != code)
    goto end;

  code = owl_renderer_image_manager_find_slot_(renderer, image);

  if (OWL_SUCCESS != code)
    goto end;

  {
    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format =
        owl_renderer_pixel_format_as_vk_format(state.format);
    image_create_info.extent.width = state.width;
    image_create_info.extent.height = state.height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = state.mips;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(vkCreateImage(renderer->device, &image_create_info, NULL,
                               &renderer->image_manager_images[image->slot]));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    vkGetImageMemoryRequirements(renderer->device,
                                 renderer->image_manager_images[image->slot],
                                 &requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, &requirements, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                         &renderer->image_manager_memories[image->slot]));
    OWL_VK_CHECK(vkBindImageMemory(
        renderer->device, renderer->image_manager_images[image->slot],
        renderer->image_manager_memories[image->slot], 0));
  }

  {
    VkImageViewCreateInfo view_create_info;

    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.pNext = NULL;
    view_create_info.flags = 0;
    view_create_info.image = renderer->image_manager_images[image->slot];
    view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_create_info.format =
        owl_renderer_pixel_format_as_vk_format(state.format);
    view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = state.mips;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(
        vkCreateImageView(renderer->device, &view_create_info, NULL,
                          &renderer->image_manager_views[image->slot]));
  }

  {
    VkDescriptorSetLayout layout;
    VkDescriptorSetAllocateInfo set_allocate_info;

    layout = renderer->sampler_image_set_layout;

    set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_allocate_info.pNext = NULL;
    set_allocate_info.descriptorPool = renderer->common_set_pool;
    set_allocate_info.descriptorSetCount = 1;
    set_allocate_info.pSetLayouts = &layout;

    OWL_VK_CHECK(
        vkAllocateDescriptorSets(renderer->device, &set_allocate_info,
                                 &renderer->image_manager_sets[image->slot]));
  }

  code = owl_renderer_begin_immidiate_command_buffer(renderer);

  if (OWL_SUCCESS != code)
    goto end;
  {
    struct owl_renderer_image_transition_desc transition_desc;

    transition_desc.mips = state.mips;
    transition_desc.layers = 1;
    transition_desc.src_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    transition_desc.dst_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    owl_renderer_image_transition_(renderer, image, &transition_desc);
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

    vkCmdCopyBufferToImage(renderer->immidiate_command_buffer,
                           state.reference.buffer,
                           renderer->image_manager_images[image->slot],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  }

  {
    struct owl_renderer_image_generate_mips_desc mips_desc;

    mips_desc.width = (owl_i32)state.width;
    mips_desc.height = (owl_i32)state.height;
    mips_desc.mips = state.mips;

    owl_renderer_image_generate_mips_(renderer, image, &mips_desc);
  }

  code = owl_renderer_end_immidiate_command_buffer(renderer);

  if (OWL_SUCCESS != code)
    goto end;

  {
    VkSamplerCreateInfo sampler_create_info;

    if (desc->use_default_sampler) {
      sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_create_info.pNext = NULL;
      sampler_create_info.flags = 0;
      sampler_create_info.magFilter = VK_FILTER_LINEAR;
      sampler_create_info.minFilter = VK_FILTER_LINEAR;
      sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      sampler_create_info.mipLodBias = 0.0F;
      sampler_create_info.anisotropyEnable = VK_TRUE;
      sampler_create_info.maxAnisotropy = 16.0F;
      sampler_create_info.compareEnable = VK_FALSE;
      sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler_create_info.minLod = 0.0F;
      sampler_create_info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    } else {

      sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      sampler_create_info.pNext = NULL;
      sampler_create_info.flags = 0;
      sampler_create_info.magFilter =
          owl_renderer_sampler_filter_as_vk_filter_(desc->sampler_mag_filter);
      sampler_create_info.minFilter =
          owl_renderer_sampler_filter_as_vk_filter_(desc->sampler_min_filter);
      sampler_create_info.mipmapMode =
          owl_renderer_sampler_mip_mode_as_vk_sampler_mipmap_mode_(
              desc->sampler_mip_mode);
      sampler_create_info.addressModeU =
          owl_renderer_sampler_addr_mode_as_vk_sampler_address_mode_(
              desc->sampler_wrap_u);
      sampler_create_info.addressModeV =
          owl_renderer_sampler_addr_mode_as_vk_sampler_address_mode_(
              desc->sampler_wrap_v);
      sampler_create_info.addressModeW =
          owl_renderer_sampler_addr_mode_as_vk_sampler_address_mode_(
              desc->sampler_wrap_w);
      sampler_create_info.mipLodBias = 0.0F;
      sampler_create_info.anisotropyEnable = VK_TRUE;
      sampler_create_info.maxAnisotropy = 16.0F;
      sampler_create_info.compareEnable = VK_FALSE;
      sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
      sampler_create_info.minLod = 0.0F;
      sampler_create_info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      sampler_create_info.unnormalizedCoordinates = VK_FALSE;
    }

    OWL_VK_CHECK(
        vkCreateSampler(renderer->device, &sampler_create_info, NULL,
                        &renderer->image_manager_samplers[image->slot]));
  }

  {
    VkDescriptorImageInfo image_descriptor;
    VkDescriptorImageInfo sampler_descriptor;
    VkWriteDescriptorSet writes[2];

    sampler_descriptor.sampler = renderer->image_manager_samplers[image->slot];
    sampler_descriptor.imageView = VK_NULL_HANDLE;
    sampler_descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image_descriptor.sampler = VK_NULL_HANDLE;
    image_descriptor.imageView = renderer->image_manager_views[image->slot];
    image_descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = renderer->image_manager_sets[image->slot];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &sampler_descriptor;
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = renderer->image_manager_sets[image->slot];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &image_descriptor;
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, OWL_ARRAY_SIZE(writes), writes, 0,
                           NULL);
  }

  owl_renderer_clear_dynamic_heap_offset(renderer);

end:
  return code;
}

void owl_renderer_deinit_image(struct owl_renderer *renderer,
                               struct owl_renderer_image *image) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));
  OWL_ASSERT(renderer->image_manager_slots[image->slot]);

  renderer->image_manager_slots[image->slot] = 0;

  vkFreeDescriptorSets(renderer->device, renderer->common_set_pool, 1,
                       &renderer->image_manager_sets[image->slot]);
  vkDestroySampler(renderer->device,
                   renderer->image_manager_samplers[image->slot], NULL);
  vkDestroyImageView(renderer->device,
                     renderer->image_manager_views[image->slot], NULL);
  vkFreeMemory(renderer->device, renderer->image_manager_memories[image->slot],
               NULL);
  vkDestroyImage(renderer->device, renderer->image_manager_images[image->slot],
                 NULL);
}

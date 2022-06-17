#include "owl_renderer.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_vector.h"
#include "owl_vector_math.h"
#include "stb_truetype.h"

#define OWL_DEFAULT_BUFFER_SIZE (1 << 16)

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32
owl_renderer_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                            VkDebugUtilsMessageTypeFlagsEXT type,
                            VkDebugUtilsMessengerCallbackDataEXT const *data,
                            void *user) {
  owl_unused(type);
  owl_unused(user);

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

owl_private char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif

owl_private owl_code
owl_renderer_init_instance(struct owl_renderer *renderer) {
  owl_code code = OWL_OK;
  VkResult vk_result = VK_SUCCESS;

  {
    uint32_t num_extensions;
    char const *const *extensions;

    VkApplicationInfo application_info;
    VkInstanceCreateInfo instance_info;

    code = owl_plataform_get_required_instance_extensions(
        renderer->plataform, &num_extensions, &extensions);
    if (code)
      return code;

    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = NULL;
    application_info.pApplicationName =
        owl_plataform_get_title(renderer->plataform);
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "No Engine";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pNext = NULL;
    instance_info.flags = 0;
    instance_info.pApplicationInfo = &application_info;
#if defined(OWL_ENABLE_VALIDATION)
    instance_info.enabledLayerCount = owl_array_size(debug_validation_layers);
    instance_info.ppEnabledLayerNames = debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
    instance_info.enabledLayerCount = 0;
    instance_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
    instance_info.enabledExtensionCount = num_extensions;
    instance_info.ppEnabledExtensionNames = extensions;

    vk_result = vkCreateInstance(&instance_info, NULL, &renderer->instance);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

#if defined(OWL_ENABLE_VALIDATION)
  {
    VkDebugUtilsMessengerCreateInfoEXT debug_info;

    renderer->vk_create_debug_utils_messenger_ext =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            renderer->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!renderer->vk_create_debug_utils_messenger_ext) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_instance;
    }

    renderer->vk_destroy_debug_utils_messenger_ext =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            renderer->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (!renderer->vk_destroy_debug_utils_messenger_ext) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_instance;
    }

    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.pNext = NULL;
    debug_info.flags = 0;
    debug_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = owl_renderer_debug_callback;
    debug_info.pUserData = renderer;

    vk_result = renderer->vk_create_debug_utils_messenger_ext(
        renderer->instance, &debug_info, NULL, &renderer->debug);
    if (vk_result)
      goto error_destroy_instance;
  }
#endif

  goto out;

#if defined(OWL_ENABLE_VALIDATION)
error_destroy_instance:
  vkDestroyInstance(renderer->instance, NULL);
#endif

out:
  return code;
}

owl_private void
owl_renderer_deinit_instance(struct owl_renderer *renderer) {
#if defined(OWL_ENABLE_VALIDATION)
  renderer->vk_destroy_debug_utils_messenger_ext(renderer->instance,
                                                 renderer->debug, NULL);
#endif
  vkDestroyInstance(renderer->instance, NULL);
}

owl_private owl_code
owl_renderer_init_surface(struct owl_renderer *renderer) {
  return owl_plataform_create_vulkan_surface(renderer->plataform, renderer);
}

owl_private void
owl_renderer_deinit_surface(struct owl_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

owl_private char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

owl_private int
owl_renderer_validate_queue_families(struct owl_renderer *renderer) {
  if ((uint32_t)-1 == renderer->graphics_queue_family)
    return 0;

  if ((uint32_t)-1 == renderer->present_queue_family)
    return 0;

  return 1;
}

owl_private int
owl_renderer_ensure_queue_families(struct owl_renderer *renderer) {
  uint32_t i;
  VkResult vk_result = VK_SUCCESS;
  uint32_t num_properties = 0;
  VkQueueFamilyProperties *properties = NULL;

  renderer->graphics_queue_family = (uint32_t)-1;
  renderer->present_queue_family = (uint32_t)-1;

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &num_properties, NULL);

  properties = owl_malloc(num_properties * sizeof(*properties));
  if (!properties)
    return 0;

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &num_properties, properties);

  for (i = 0; i < num_properties; ++i) {
    VkBool32 has_surface;

    vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(
        renderer->physical_device, i, renderer->surface, &has_surface);
    if (vk_result)
      goto cleanup;

    if (has_surface)
      renderer->present_queue_family = i;

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags)
      renderer->graphics_queue_family = i;

    if (owl_renderer_validate_queue_families(renderer))
      goto cleanup;
  }

cleanup:
  owl_free(properties);

  return owl_renderer_validate_queue_families(renderer);
}

owl_private int
owl_renderer_validate_device_extensions(struct owl_renderer *renderer) {
  int found = 0;
  uint32_t i = 0;
  uint32_t num_properties = 0;
  VkExtensionProperties *properties = NULL;
  int *state = NULL;
  VkResult vk_result = VK_SUCCESS;

  vk_result = vkEnumerateDeviceExtensionProperties(
      renderer->physical_device, NULL, &num_properties, NULL);
  if (vk_result)
    return 0;

  properties = owl_malloc(num_properties * sizeof(*properties));
  if (!properties)
    return 0;

  vk_result = vkEnumerateDeviceExtensionProperties(
      renderer->physical_device, NULL, &num_properties, properties);
  if (vk_result)
    goto error_free_properties;

  state = owl_malloc(owl_array_size(device_extensions) * sizeof(*state));
  if (!state)
    goto error_free_state;

  for (i = 0; i < owl_array_size(device_extensions); ++i)
    state[i] = 0;

  for (i = 0; i < num_properties; ++i) {
    uint32_t j;
    for (j = 0; j < owl_array_size(device_extensions); ++j) {
      size_t const max_extension_length = VK_MAX_EXTENSION_NAME_SIZE;
      char const *l = device_extensions[j];
      char const *r = properties[j].extensionName;

      uint32_t minlen = owl_min(max_extension_length, owl_strlen(l));

      state[j] = !owl_strncmp(l, r, minlen);
    }
  }

  found = 1;
  for (i = 0; i < owl_array_size(device_extensions) && found; ++i)
    if (!state[i])
      found = 0;

error_free_state:
  owl_free(state);

error_free_properties:
  owl_free(properties);

  return found;
}

owl_private owl_code
owl_renderer_ensure_surface_format(struct owl_renderer *renderer,
                                   VkFormat format,
                                   VkColorSpaceKHR color_space) {
  uint32_t i;
  uint32_t num_formats = 0;
  VkSurfaceFormatKHR *formats = NULL;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_ERROR_NOT_FOUND;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &num_formats, NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  formats = owl_malloc(num_formats * sizeof(*formats));
  if (!formats)
    return OWL_ERROR_NO_MEMORY;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &num_formats, formats);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_formats;
  }

  for (i = 0; i < num_formats && OWL_ERROR_NOT_FOUND == code; ++i)
    if (formats[i].format == format && formats[i].colorSpace == color_space)
      code = OWL_OK;

  if (!code) {
    renderer->surface_format.format = format;
    renderer->surface_format.colorSpace = color_space;
  }

error_free_formats:
  owl_free(formats);

  return code;
}

owl_private owl_code
owl_renderer_ensure_msaa(struct owl_renderer *renderer,
                         VkSampleCountFlagBits msaa) {
  VkPhysicalDeviceProperties properties;
  VkSampleCountFlagBits limit = ~0U;

  owl_code code = OWL_OK;

  vkGetPhysicalDeviceProperties(renderer->physical_device, &properties);

  limit &= properties.limits.framebufferColorSampleCounts;
  limit &= properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_1_BIT & msaa) {
    code = OWL_ERROR_NOT_FOUND;
  } else if (limit & msaa) {
    renderer->msaa = msaa;
    code = OWL_OK;
  } else {
    renderer->msaa = VK_SAMPLE_COUNT_2_BIT;
    code = OWL_ERROR_NOT_FOUND;
  }

  return code;
}

owl_private int
owl_renderer_validate_physical_device(struct owl_renderer *renderer) {
  int has_fams;
  int has_ext_props;
  uint32_t has_formats;
  uint32_t has_modes;
  VkPhysicalDeviceFeatures feats;
  VkResult vk_result;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &has_formats, NULL);
  if (vk_result || !has_formats)
    return 0;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &has_modes, NULL);
  if (vk_result || !has_modes)
    return 0;

  has_fams = owl_renderer_ensure_queue_families(renderer);
  if (!has_fams)
    return 0;

  has_ext_props = owl_renderer_validate_device_extensions(renderer);
  if (!has_ext_props)
    return 0;

  vkGetPhysicalDeviceFeatures(renderer->physical_device, &feats);
  if (!feats.samplerAnisotropy)
    return 0;

  return 1;
}

owl_private owl_code
owl_renderer_ensure_physical_device(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t num_devices = 0;
  VkPhysicalDevice *devices = NULL;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  vk_result =
      vkEnumeratePhysicalDevices(renderer->instance, &num_devices, NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  devices = owl_malloc(num_devices * sizeof(*devices));

  if (!devices)
    return OWL_ERROR_NO_MEMORY;

  vk_result =
      vkEnumeratePhysicalDevices(renderer->instance, &num_devices, devices);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_devices;
  }

  for (i = 0; i < num_devices; ++i) {
    renderer->physical_device = devices[i];

    if (owl_renderer_validate_physical_device(renderer))
      break;
  }

  if (!owl_renderer_validate_queue_families(renderer)) {
    code = OWL_ERROR_FATAL;
    goto error_free_devices;
  }

error_free_devices:
  owl_free(devices);

  return code;
}

owl_private owl_code
owl_renderer_init_device(struct owl_renderer *renderer) {
  VkPhysicalDeviceFeatures features;
  VkDeviceCreateInfo device_info;
  VkDeviceQueueCreateInfo queue_infos[2];
  float const priority = 1.0F;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  code = owl_renderer_ensure_physical_device(renderer);
  if (code)
    return code;

  code = owl_renderer_ensure_msaa(renderer, VK_SAMPLE_COUNT_2_BIT);
  if (code)
    return code;

  code = owl_renderer_ensure_surface_format(renderer, VK_FORMAT_B8G8R8A8_SRGB,
                                            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
  if (code)
    return code;

  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = NULL;
  queue_infos[0].flags = 0;
  queue_infos[0].queueFamilyIndex = renderer->graphics_queue_family;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &priority;

  queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[1].pNext = NULL;
  queue_infos[1].flags = 0;
  queue_infos[1].queueFamilyIndex = renderer->present_queue_family;
  queue_infos[1].queueCount = 1;
  queue_infos[1].pQueuePriorities = &priority;

  vkGetPhysicalDeviceFeatures(renderer->physical_device, &features);

  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = NULL;
  device_info.flags = 0;
  if (renderer->graphics_queue_family == renderer->present_queue_family) {
    device_info.queueCreateInfoCount = 1;
  } else {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos;
  device_info.enabledLayerCount = 0;      /* deprecated */
  device_info.ppEnabledLayerNames = NULL; /* deprecated */
  device_info.enabledExtensionCount = owl_array_size(device_extensions);
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.pEnabledFeatures = &features;

  vk_result = vkCreateDevice(renderer->physical_device, &device_info, NULL,
                             &renderer->device);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vkGetDeviceQueue(renderer->device, renderer->graphics_queue_family, 0,
                   &renderer->graphics_queue);

  vkGetDeviceQueue(renderer->device, renderer->present_queue_family, 0,
                   &renderer->present_queue);

  return code;
}

owl_private void
owl_renderer_deinit_device(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

owl_private owl_code
owl_renderer_clamp_dimensions(struct owl_renderer *renderer) {
  VkSurfaceCapabilitiesKHR capabilities;
  VkResult vk_result;

  vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      renderer->physical_device, renderer->surface, &capabilities);
  if (vk_result)
    return OWL_ERROR_FATAL;

  if ((uint32_t)-1 != capabilities.currentExtent.width) {
    uint32_t const min_width = capabilities.minImageExtent.width;
    uint32_t const max_width = capabilities.maxImageExtent.width;
    uint32_t const min_height = capabilities.minImageExtent.height;
    uint32_t const max_height = capabilities.maxImageExtent.height;

    renderer->width = owl_clamp(renderer->width, min_width, max_width);
    renderer->height = owl_clamp(renderer->height, min_height, max_height);
  }

  return OWL_OK;
}

owl_private owl_code
owl_renderer_init_attachments(struct owl_renderer *renderer) {
  owl_code code = OWL_OK;
  VkResult vk_result = VK_SUCCESS;

  {
    VkImageCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = 0;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = renderer->surface_format.format;
    image_info.extent.width = renderer->width;
    image_info.extent.height = renderer->height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = renderer->msaa;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &image_info, NULL,
                              &renderer->color_image);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetImageMemoryRequirements(renderer->device, renderer->color_image,
                                 &memory_requirements);

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                                 &renderer->color_memory);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_color_image;
    }

    vk_result = vkBindImageMemory(renderer->device, renderer->color_image,
                                  renderer->color_memory, 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_color_memory;
    }
  }

  {
    VkImageViewCreateInfo image_view_info;

    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.pNext = NULL;
    image_view_info.flags = 0;
    image_view_info.image = renderer->color_image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = renderer->surface_format.format;
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_view_info, NULL,
                                  &renderer->color_image_view);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_color_memory;
    }
  }

  {
    VkFormatProperties d24_unorm_s8_uint_properties;
    VkFormatProperties d32_sfloat_s8_uint_properties;
    VkFormatFeatureFlagBits required_features;

    required_features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vkGetPhysicalDeviceFormatProperties(renderer->physical_device,
                                        VK_FORMAT_D24_UNORM_S8_UINT,
                                        &d24_unorm_s8_uint_properties);

    vkGetPhysicalDeviceFormatProperties(renderer->physical_device,
                                        VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        &d32_sfloat_s8_uint_properties);

    if (required_features &
        d24_unorm_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    } else if (required_features &
               d32_sfloat_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    } else {
      code = OWL_ERROR_FATAL;
      goto error_destroy_color_image_view;
    }
  }

  {

    VkImageCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = 0;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = renderer->depth_format;
    image_info.extent.width = renderer->width;
    image_info.extent.height = renderer->height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = renderer->msaa;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = NULL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &image_info, NULL,
                              &renderer->depth_image);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_color_image_view;
    }
  }

  {
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetImageMemoryRequirements(renderer->device, renderer->depth_image,
                                 &memory_requirements);

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                                 &renderer->depth_memory);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_depth_image;
    }

    vk_result = vkBindImageMemory(renderer->device, renderer->depth_image,
                                  renderer->depth_memory, 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_depth_memory;
    }
  }
  {
    VkImageViewCreateInfo image_view_info;

    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.pNext = NULL;
    image_view_info.flags = 0;
    image_view_info.image = renderer->depth_image;
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = renderer->depth_format;
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_view_info, NULL,
                                  &renderer->depth_image_view);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_depth_memory;
    }
  }

  goto out;

error_free_depth_memory:
  vkFreeMemory(renderer->device, renderer->depth_memory, NULL);

error_destroy_depth_image:
  vkDestroyImage(renderer->device, renderer->depth_image, NULL);

error_destroy_color_image_view:
  vkDestroyImageView(renderer->device, renderer->color_image_view, NULL);

error_free_color_memory:
  vkFreeMemory(renderer->device, renderer->color_memory, NULL);

error_destroy_color_image:
  vkDestroyImage(renderer->device, renderer->color_image, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_attachments(struct owl_renderer *renderer) {
  vkDestroyImageView(renderer->device, renderer->depth_image_view, NULL);
  vkFreeMemory(renderer->device, renderer->depth_memory, NULL);
  vkDestroyImage(renderer->device, renderer->depth_image, NULL);
  vkDestroyImageView(renderer->device, renderer->color_image_view, NULL);
  vkFreeMemory(renderer->device, renderer->color_memory, NULL);
  vkDestroyImage(renderer->device, renderer->color_image, NULL);
}

owl_private owl_code
owl_renderer_init_render_passes(struct owl_renderer *renderer) {
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo render_pass_info;
  VkResult vk_result;

  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  resolve_reference.attachment = 2;
  resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  attachments[0].flags = 0;
  attachments[0].format = renderer->surface_format.format;
  attachments[0].samples = renderer->msaa;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  attachments[1].flags = 0;
  attachments[1].format = renderer->depth_format;
  attachments[1].samples = renderer->msaa;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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
  subpass.pColorAttachments = &color_reference;
  subpass.pResolveAttachments = &resolve_reference;
  subpass.pDepthStencilAttachment = &depth_reference;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = 0;

  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.pNext = NULL;
  render_pass_info.flags = 0;
  render_pass_info.attachmentCount = owl_array_size(attachments);
  render_pass_info.pAttachments = attachments;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  vk_result = vkCreateRenderPass(renderer->device, &render_pass_info, NULL,
                                 &renderer->main_render_pass);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_private void
owl_renderer_deinit_render_passes(struct owl_renderer *renderer) {
  vkDestroyRenderPass(renderer->device, renderer->main_render_pass, NULL);
}

#define OWL_MAX_PRESENT_MODES 16

owl_private owl_code
owl_renderer_ensure_present_mode(struct owl_renderer *renderer,
                                 VkPresentModeKHR mode) {
  int i;
  uint32_t num_modes;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_ERROR_NOT_FOUND;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &num_modes, NULL);
  if (vk_result || OWL_MAX_PRESENT_MODES <= num_modes)
    return OWL_ERROR_FATAL;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &num_modes, modes);
  if (vk_result)
    return OWL_ERROR_FATAL;

  for (i = 0; i < (int)num_modes && OWL_ERROR_NOT_FOUND == code; ++i)
    if (modes[i] == mode)
      code = OWL_OK;

  if (!code) {
    renderer->present_mode = mode;
  } else {
    renderer->present_mode = VK_PRESENT_MODE_FIFO_KHR;
  }

  return code;
}

owl_private owl_code
owl_renderer_init_swapchain(struct owl_renderer *renderer) {
  int i;
  VkResult vk_result;
  owl_code code = OWL_OK;

  renderer->image = 0;

  code = owl_renderer_ensure_present_mode(renderer, VK_PRESENT_MODE_FIFO_KHR);
  if (code)
    goto out;

  {
    uint32_t families[2];
    VkSwapchainCreateInfoKHR swapchain_info;
    VkSurfaceCapabilitiesKHR capabilities;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        renderer->physical_device, renderer->surface, &capabilities);

    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.pNext = NULL;
    swapchain_info.flags = 0;
    swapchain_info.surface = renderer->surface;
    swapchain_info.minImageCount = renderer->num_frames;
    swapchain_info.imageFormat = renderer->surface_format.format;
    swapchain_info.imageColorSpace = renderer->surface_format.colorSpace;
    swapchain_info.imageExtent.width = renderer->width;
    swapchain_info.imageExtent.height = renderer->height;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = renderer->present_mode;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    families[0] = renderer->graphics_queue_family;
    families[1] = renderer->present_queue_family;

    if (renderer->graphics_queue_family == renderer->present_queue_family) {
      swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain_info.queueFamilyIndexCount = 0;
      swapchain_info.pQueueFamilyIndices = NULL;
    } else {
      swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      swapchain_info.queueFamilyIndexCount = 2;
      swapchain_info.pQueueFamilyIndices = families;
    }

    vk_result = vkCreateSwapchainKHR(renderer->device, &swapchain_info, NULL,
                                     &renderer->swapchain);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }

    vk_result = vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                        &renderer->num_images, NULL);
    if (vk_result || OWL_MAX_SWAPCHAIN_IMAGES <= renderer->num_images) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_swapchain;
    }

    vk_result =
        vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                &renderer->num_images, renderer->images);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_swapchain;
    }
  }

  for (i = 0; i < (int)renderer->num_images; ++i) {
    VkImageViewCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = 0;
    image_info.image = renderer->images[i];
    image_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_info.format = renderer->surface_format.format;
    image_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_info.subresourceRange.baseMipLevel = 0;
    image_info.subresourceRange.levelCount = 1;
    image_info.subresourceRange.baseArrayLayer = 0;
    image_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_info, NULL,
                                  &renderer->image_views[i]); /**/
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_views;
    }
  }

  for (i = 0; i < (int)renderer->num_images; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo framebuffer_info;

    attachments[0] = renderer->color_image_view;
    attachments[1] = renderer->depth_image_view;
    attachments[2] = renderer->image_views[i];

    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.pNext = NULL;
    framebuffer_info.flags = 0;
    framebuffer_info.renderPass = renderer->main_render_pass;
    framebuffer_info.attachmentCount = owl_array_size(attachments);
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = renderer->width;
    framebuffer_info.height = renderer->height;
    framebuffer_info.layers = 1;

    vk_result = vkCreateFramebuffer(renderer->device, &framebuffer_info, NULL,
                                    &renderer->framebuffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_framebuffers;
    }
  }

  goto out;

error_destroy_framebuffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFramebuffer(renderer->device, renderer->framebuffers[i], NULL);

  i = renderer->num_images;

error_destroy_image_views:
  for (i = i - 1; i >= 0; --i)
    vkDestroyImageView(renderer->device, renderer->image_views[i], NULL);

error_destroy_swapchain:
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_swapchain(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->num_images; ++i)
    vkDestroyFramebuffer(renderer->device, renderer->framebuffers[i], NULL);

  for (i = 0; i < renderer->num_images; ++i)
    vkDestroyImageView(renderer->device, renderer->image_views[i], NULL);

  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
}

owl_private owl_code
owl_renderer_init_pools(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  {
    VkCommandPoolCreateInfo command_pool_info;

    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    command_pool_info.queueFamilyIndex = renderer->graphics_queue_family;

    vk_result = vkCreateCommandPool(renderer->device, &command_pool_info, NULL,
                                    &renderer->command_pool);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkDescriptorPoolSize sizes[6];
    VkDescriptorPoolCreateInfo descriptor_pool_info;
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

    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.pNext = NULL;
    descriptor_pool_info.flags =
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_info.maxSets = 256 * owl_array_size(sizes);
    descriptor_pool_info.poolSizeCount = owl_array_size(sizes);
    descriptor_pool_info.pPoolSizes = sizes;

    vk_result = vkCreateDescriptorPool(renderer->device, &descriptor_pool_info,
                                       NULL, &renderer->descriptor_pool);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_command_pool;
    }
  }

  goto out;

error_destroy_command_pool:
  vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_pools(struct owl_renderer *renderer) {
  vkDestroyDescriptorPool(renderer->device, renderer->descriptor_pool, NULL);
  vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);
}

owl_private owl_code
owl_renderer_init_shaders(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  {
    VkShaderModuleCreateInfo shader_info;

    /* TODO(samuel): Properly load code at runtime */
    owl_local_persist uint32_t const spv[] = {
#include "owl_basic.vert.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->basic_vertex_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_basic.frag.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->basic_fragment_shader);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_basic_vertex_shader;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_font.frag.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->text_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_basic_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_model.vert.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->model_vertex_shader);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_text_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_model.frag.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->model_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_vertex_shader;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_skybox.vert.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->skybox_vertex_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo shader_info;

    owl_local_persist uint32_t const spv[] = {
#include "owl_skybox.frag.spv.u32"
    };

    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = NULL;
    shader_info.flags = 0;
    shader_info.codeSize = sizeof(spv);
    shader_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_info, NULL,
                                     &renderer->skybox_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_skybox_vertex_shader;
    }
  }

  goto out;

error_destroy_skybox_vertex_shader:
  vkDestroyShaderModule(renderer->device, renderer->skybox_vertex_shader,
                        NULL);

error_destroy_model_fragment_shader:
  vkDestroyShaderModule(renderer->device, renderer->model_fragment_shader,
                        NULL);

error_destroy_model_vertex_shader:
  vkDestroyShaderModule(renderer->device, renderer->model_vertex_shader, NULL);

error_destroy_text_fragment_shader:
  vkDestroyShaderModule(renderer->device, renderer->text_fragment_shader,
                        NULL);

error_destroy_basic_fragment_shader:
  vkDestroyShaderModule(renderer->device, renderer->basic_fragment_shader,
                        NULL);

error_destroy_basic_vertex_shader:
  vkDestroyShaderModule(renderer->device, renderer->basic_vertex_shader, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_shaders(struct owl_renderer *renderer) {
  vkDestroyShaderModule(renderer->device, renderer->skybox_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->skybox_vertex_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->model_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->model_vertex_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->text_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_fragment_shader,
                        NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_vertex_shader, NULL);
}

owl_private owl_code
owl_renderer_init_layouts(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = NULL;
    set_layout_info.flags = 0;
    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;

    vk_result =
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_info, NULL,
                                    &renderer->ubo_vertex_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = NULL;
    set_layout_info.flags = 0;
    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;

    vk_result =
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_info, NULL,
                                    &renderer->ubo_fragment_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_vertex_set_layout;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags =
        VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = NULL;
    set_layout_info.flags = 0;
    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;

    vk_result =
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_info, NULL,
                                    &renderer->ubo_both_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_fragment_set_layout;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = NULL;
    set_layout_info.flags = 0;
    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;

    vk_result =
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_info, NULL,
                                    &renderer->ssbo_vertex_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_both_set_layout;
    }
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo set_layout_info;

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

    set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pNext = NULL;
    set_layout_info.flags = 0;
    set_layout_info.bindingCount = owl_array_size(bindings);
    set_layout_info.pBindings = bindings;

    vk_result =
        vkCreateDescriptorSetLayout(renderer->device, &set_layout_info, NULL,
                                    &renderer->image_fragment_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ssbo_vertex_set_layout;
    }
  }

  {
    VkDescriptorSetLayout set_layouts[2];
    VkPipelineLayoutCreateInfo pipeline_layout_info;

    set_layouts[0] = renderer->ubo_vertex_set_layout;
    set_layouts[1] = renderer->image_fragment_set_layout;

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = NULL;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount = owl_array_size(set_layouts);
    pipeline_layout_info.pSetLayouts = set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    vk_result =
        vkCreatePipelineLayout(renderer->device, &pipeline_layout_info, NULL,
                               &renderer->common_pipeline_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_fragment_set_layout;
    }
  }

  {
    VkDescriptorSetLayout set_layouts[6];
    VkPushConstantRange push_constant_range;
    VkPipelineLayoutCreateInfo pipeline_layout_info;

    push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(struct owl_model_material_push_constant);

    set_layouts[0] = renderer->ubo_both_set_layout;
    set_layouts[1] = renderer->image_fragment_set_layout;
    set_layouts[2] = renderer->image_fragment_set_layout;
    set_layouts[3] = renderer->image_fragment_set_layout;
    set_layouts[4] = renderer->ssbo_vertex_set_layout;
    set_layouts[5] = renderer->ubo_fragment_set_layout;

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = NULL;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount = owl_array_size(set_layouts);
    pipeline_layout_info.pSetLayouts = set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    vk_result = vkCreatePipelineLayout(renderer->device, &pipeline_layout_info,
                                       NULL, &renderer->model_pipeline_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_common_pipeline_layout;
    }
  }

  goto out;

error_destroy_common_pipeline_layout:
  vkDestroyPipelineLayout(renderer->device, renderer->common_pipeline_layout,
                          NULL);

error_destroy_image_fragment_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->image_fragment_set_layout, NULL);

error_destroy_ssbo_vertex_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ssbo_vertex_set_layout, NULL);

error_destroy_ubo_both_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device, renderer->ubo_both_set_layout,
                               NULL);

error_destroy_ubo_fragment_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_fragment_set_layout, NULL);

error_destroy_ubo_vertex_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_vertex_set_layout, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_layouts(struct owl_renderer *renderer) {
  vkDestroyPipelineLayout(renderer->device, renderer->model_pipeline_layout,
                          NULL);
  vkDestroyPipelineLayout(renderer->device, renderer->common_pipeline_layout,
                          NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->image_fragment_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ssbo_vertex_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device, renderer->ubo_both_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_fragment_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_vertex_set_layout, NULL);
}

static owl_code
owl_renderer_init_pipelines(struct owl_renderer *renderer) {
  VkResult vk_result;
  owl_code code = OWL_OK;

  {
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rast;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_info;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_pcu_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_pcu_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_pcu_vertex, color);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_pcu_vertex, uv);

    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext = NULL;
    vertex_input.flags = 0;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount =
        owl_array_size(vertex_attributes);
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->width;
    viewport.height = renderer->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderer->width;
    scissor.extent.height = renderer->height;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = NULL;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.pNext = NULL;
    rast.flags = 0;
    rast.depthClampEnable = VK_FALSE;
    rast.rasterizerDiscardEnable = VK_FALSE;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_BACK_BIT;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.depthBiasEnable = VK_FALSE;
    rast.depthBiasConstantFactor = 0.0F;
    rast.depthBiasClamp = 0.0F;
    rast.depthBiasSlopeFactor = 0.0F;
    rast.lineWidth = 1.0F;

    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext = NULL;
    multisample.flags = 0;
    multisample.rasterizationSamples = renderer->msaa;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_attachment;
    color.blendConstants[0] = 0.0F;
    color.blendConstants[1] = 0.0F;
    color.blendConstants[2] = 0.0F;
    color.blendConstants[3] = 0.0F;

    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.pNext = NULL;
    depth.flags = 0;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

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

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = owl_array_size(stages);
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rast;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth;
    pipeline_info.pColorBlendState = &color;
    pipeline_info.pDynamicState = NULL;
    pipeline_info.layout = renderer->common_pipeline_layout;
    pipeline_info.renderPass = renderer->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                          &pipeline_info, NULL,
                                          &renderer->basic_pipeline);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_info;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_pcu_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_pcu_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_pcu_vertex, color);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_pcu_vertex, uv);

    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext = NULL;
    vertex_input.flags = 0;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount =
        owl_array_size(vertex_attributes);
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->width;
    viewport.height = renderer->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderer->width;
    scissor.extent.height = renderer->height;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = NULL;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.pNext = NULL;
    rasterizer.flags = 0;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0F;
    rasterizer.depthBiasClamp = 0.0F;
    rasterizer.depthBiasSlopeFactor = 0.0F;
    rasterizer.lineWidth = 1.0F;

    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext = NULL;
    multisample.flags = 0;
    multisample.rasterizationSamples = renderer->msaa;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_attachment;
    color.blendConstants[0] = 0.0F;
    color.blendConstants[1] = 0.0F;
    color.blendConstants[2] = 0.0F;
    color.blendConstants[3] = 0.0F;

    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.pNext = NULL;
    depth.flags = 0;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

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

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = owl_array_size(stages);
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth;
    pipeline_info.pColorBlendState = &color;
    pipeline_info.pDynamicState = NULL;
    pipeline_info.layout = renderer->common_pipeline_layout;
    pipeline_info.renderPass = renderer->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                          &pipeline_info, NULL,
                                          &renderer->wires_pipeline);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_basic_pipeline;
    }
  }

  {
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo resterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_info;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_pcu_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_pcu_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_pcu_vertex, color);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_pcu_vertex, uv);

    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext = NULL;
    vertex_input.flags = 0;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount =
        owl_array_size(vertex_attributes);
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->width;
    viewport.height = renderer->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderer->width;
    scissor.extent.height = renderer->height;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = NULL;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

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

    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext = NULL;
    multisample.flags = 0;
    multisample.rasterizationSamples = renderer->msaa;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_attachment;
    color.blendConstants[0] = 0.0F;
    color.blendConstants[1] = 0.0F;
    color.blendConstants[2] = 0.0F;
    color.blendConstants[3] = 0.0F;

    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.pNext = NULL;
    depth.flags = 0;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

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
    stages[1].module = renderer->text_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = owl_array_size(stages);
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &resterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth;
    pipeline_info.pColorBlendState = &color;
    pipeline_info.pDynamicState = NULL;
    pipeline_info.layout = renderer->common_pipeline_layout;
    pipeline_info.renderPass = renderer->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                          &pipeline_info, NULL,
                                          &renderer->text_pipeline);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_wires_pipeline;
    }
  }

  {
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes[6];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rast;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_info;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_pnuujw_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_pnuujw_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_pnuujw_vertex, normal);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_pnuujw_vertex, uv0);

    vertex_attributes[3].binding = 0;
    vertex_attributes[3].location = 3;
    vertex_attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[3].offset = offsetof(struct owl_pnuujw_vertex, uv1);

    vertex_attributes[4].binding = 0;
    vertex_attributes[4].location = 4;
    vertex_attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attributes[4].offset = offsetof(struct owl_pnuujw_vertex, joints0);

    vertex_attributes[5].binding = 0;
    vertex_attributes[5].location = 5;
    vertex_attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attributes[5].offset = offsetof(struct owl_pnuujw_vertex, weights0);

    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext = NULL;
    vertex_input.flags = 0;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount =
        owl_array_size(vertex_attributes);
    vertex_input.pVertexAttributeDescriptions = vertex_attributes;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->width;
    viewport.height = renderer->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderer->width;
    scissor.extent.height = renderer->height;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = NULL;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.pNext = NULL;
    rast.flags = 0;
    rast.depthClampEnable = VK_FALSE;
    rast.rasterizerDiscardEnable = VK_FALSE;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_BACK_BIT;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.depthBiasEnable = VK_FALSE;
    rast.depthBiasConstantFactor = 0.0F;
    rast.depthBiasClamp = 0.0F;
    rast.depthBiasSlopeFactor = 0.0F;
    rast.lineWidth = 1.0F;

    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext = NULL;
    multisample.flags = 0;
    multisample.rasterizationSamples = renderer->msaa;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_attachment;
    color.blendConstants[0] = 0.0F;
    color.blendConstants[1] = 0.0F;
    color.blendConstants[2] = 0.0F;
    color.blendConstants[3] = 0.0F;

    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.pNext = NULL;
    depth.flags = 0;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

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

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = owl_array_size(stages);
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rast;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth;
    pipeline_info.pColorBlendState = &color;
    pipeline_info.pDynamicState = NULL;
    pipeline_info.layout = renderer->model_pipeline_layout;
    pipeline_info.renderPass = renderer->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                          &pipeline_info, NULL,
                                          &renderer->model_pipeline);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_text_pipeline;
    }
  }

  {
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes;
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rast;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline_info;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_p_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes.binding = 0;
    vertex_attributes.location = 0;
    vertex_attributes.format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes.offset = offsetof(struct owl_p_vertex, position);

    vertex_input.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.pNext = NULL;
    vertex_input.flags = 0;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_bindings;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &vertex_attributes;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->width;
    viewport.height = renderer->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = renderer->width;
    scissor.extent.height = renderer->height;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.pNext = NULL;
    viewport_state.flags = 0;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    rast.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.pNext = NULL;
    rast.flags = 0;
    rast.depthClampEnable = VK_FALSE;
    rast.rasterizerDiscardEnable = VK_FALSE;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode = VK_CULL_MODE_BACK_BIT;
    rast.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rast.depthBiasEnable = VK_FALSE;
    rast.depthBiasConstantFactor = 0.0F;
    rast.depthBiasClamp = 0.0F;
    rast.depthBiasSlopeFactor = 0.0F;
    rast.lineWidth = 1.0F;

    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.pNext = NULL;
    multisample.flags = 0;
    multisample.rasterizationSamples = renderer->msaa;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_attachment;
    color.blendConstants[0] = 0.0F;
    color.blendConstants[1] = 0.0F;
    color.blendConstants[2] = 0.0F;
    color.blendConstants[3] = 0.0F;

    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.pNext = NULL;
    depth.flags = 0;
    depth.depthTestEnable = VK_FALSE;
    depth.depthWriteEnable = VK_FALSE;
    depth.depthCompareOp = VK_COMPARE_OP_NEVER;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = renderer->skybox_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->skybox_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = owl_array_size(stages);
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rast;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth;
    pipeline_info.pColorBlendState = &color;
    pipeline_info.pDynamicState = NULL;
    pipeline_info.layout = renderer->common_pipeline_layout;
    pipeline_info.renderPass = renderer->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                          &pipeline_info, NULL,
                                          &renderer->skybox_pipeline);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_pipeline;
    }
  }

  goto out;

error_destroy_model_pipeline:
  vkDestroyPipeline(renderer->device, renderer->model_pipeline, NULL);

error_destroy_text_pipeline:
  vkDestroyPipeline(renderer->device, renderer->text_pipeline, NULL);

error_destroy_wires_pipeline:
  vkDestroyPipeline(renderer->device, renderer->wires_pipeline, NULL);

error_destroy_basic_pipeline:
  vkDestroyPipeline(renderer->device, renderer->basic_pipeline, NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_pipelines(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device, renderer->skybox_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->model_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->text_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->wires_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->basic_pipeline, NULL);
}

owl_public owl_code
owl_renderer_init_upload_buffer(struct owl_renderer *renderer, uint64_t size) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  renderer->upload_buffer_data = NULL;
  renderer->upload_buffer_size = size;

  {
    VkBufferCreateInfo buffer_info;
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = NULL;
    buffer_info.flags = 0;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = 0;
    buffer_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_info, NULL,
                               &renderer->upload_buffer);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetBufferMemoryRequirements(renderer->device, renderer->upload_buffer,
                                  &memory_requirements);

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                                 &renderer->upload_buffer_memory);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_buffer;
    }

    vk_result = vkBindBufferMemory(renderer->device, renderer->upload_buffer,
                                   renderer->upload_buffer_memory, 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memory;
    }

    vk_result = vkMapMemory(renderer->device, renderer->upload_buffer_memory,
                            0, renderer->upload_buffer_size, 0,
                            &renderer->upload_buffer_data);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memory;
    }
  }

  goto out;

error_free_memory:
  vkFreeMemory(renderer->device, renderer->upload_buffer_memory, NULL);

error_destroy_buffer:
  vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);

out:
  return code;
}

owl_public void
owl_renderer_deinit_upload_buffer(struct owl_renderer *renderer) {
  vkFreeMemory(renderer->device, renderer->upload_buffer_memory, NULL);
  vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);
}

owl_public owl_code
owl_renderer_init_render_buffers(struct owl_renderer *renderer,
                                 uint64_t size) {
  int i;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  renderer->render_buffer_size = size;
  renderer->render_buffer_offset = 0;

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkBufferCreateInfo buffer_info;

    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = NULL;
    buffer_info.flags = 0;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_info.queueFamilyIndexCount = 0;
    buffer_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_info, NULL,
                               &renderer->render_buffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_buffers;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkMemoryAllocateInfo memory_info;
    VkMemoryRequirements memory_requirements;

    vkGetBufferMemoryRequirements(
        renderer->device, renderer->render_buffers[0], &memory_requirements);

    /* NOTE(samuel): I don't care, take the last one */
    renderer->render_buffer_alignment = memory_requirements.alignment;

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    vk_result = vkAllocateMemory(renderer->device, &memory_info, NULL,
                                 &renderer->render_buffer_memories[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }

    vk_result =
        vkBindBufferMemory(renderer->device, renderer->render_buffers[i],
                           renderer->render_buffer_memories[i], 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }

    vk_result = vkMapMemory(
        renderer->device, renderer->render_buffer_memories[i], 0,
        renderer->render_buffer_size, 0, &renderer->render_buffer_data[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSetAllocateInfo set_info;

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.pNext = NULL;
    set_info.descriptorPool = renderer->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &renderer->ubo_vertex_set_layout;

    vk_result = vkAllocateDescriptorSets(renderer->device, &set_info,
                                         &renderer->render_buffer_pvm_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_pvm_sets;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSetAllocateInfo set_info;

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.pNext = NULL;
    set_info.descriptorPool = renderer->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &renderer->ubo_both_set_layout;

    vk_result = vkAllocateDescriptorSets(
        renderer->device, &set_info, &renderer->render_buffer_model1_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_model1_sets;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSetAllocateInfo set_info;

    set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_info.pNext = NULL;
    set_info.descriptorPool = renderer->descriptor_pool;
    set_info.descriptorSetCount = 1;
    set_info.pSetLayouts = &renderer->ubo_fragment_set_layout;

    vk_result = vkAllocateDescriptorSets(
        renderer->device, &set_info, &renderer->render_buffer_model2_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_model2_sets;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorBufferInfo descriptors[3];
    VkWriteDescriptorSet writes[3];

    descriptors[0].buffer = renderer->render_buffers[i];
    descriptors[0].offset = 0;
    descriptors[0].range = sizeof(struct owl_pvm_ubo);

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = renderer->render_buffer_pvm_sets[i];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pImageInfo = NULL;
    writes[0].pBufferInfo = &descriptors[0];
    writes[0].pTexelBufferView = NULL;

    descriptors[1].buffer = renderer->render_buffers[i];
    descriptors[1].offset = 0;
    descriptors[1].range = sizeof(struct owl_model_ubo1);

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = renderer->render_buffer_model1_sets[i];
    writes[1].dstBinding = 0;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[1].pImageInfo = NULL;
    writes[1].pBufferInfo = &descriptors[1];
    writes[1].pTexelBufferView = NULL;

    descriptors[2].buffer = renderer->render_buffers[i];
    descriptors[2].offset = 0;
    descriptors[2].range = sizeof(struct owl_model_ubo2);

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].pNext = NULL;
    writes[2].dstSet = renderer->render_buffer_model2_sets[i];
    writes[2].dstBinding = 0;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[2].pImageInfo = NULL;
    writes[2].pBufferInfo = &descriptors[2];
    writes[2].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(renderer->device, owl_array_size(writes), writes, 0,
                           NULL);
  }

  goto out;

error_free_model2_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_model2_sets[i]);

  i = renderer->num_frames;

error_free_model1_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_model1_sets[i]);

  i = renderer->num_frames;

error_free_pvm_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_pvm_sets[i]);

  i = renderer->num_frames;

error_free_memories:
  for (i = i - 1; i >= 0; --i)
    vkFreeMemory(renderer->device, renderer->render_buffer_memories[i], NULL);

  i = renderer->num_frames;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(renderer->device, renderer->render_buffers[i], NULL);

out:
  return code;
}

owl_public void
owl_renderer_deinit_render_buffers(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->num_frames; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_model2_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_model1_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &renderer->render_buffer_pvm_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    vkFreeMemory(renderer->device, renderer->render_buffer_memories[i], NULL);

  for (i = 0; i < renderer->num_frames; ++i)
    vkDestroyBuffer(renderer->device, renderer->render_buffers[i], NULL);
}

owl_private owl_code
owl_renderer_init_frames(struct owl_renderer *renderer) {
  int i;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  renderer->frame = 0;

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkCommandPoolCreateInfo command_pool_info;

    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = 0;
    command_pool_info.queueFamilyIndex = renderer->graphics_queue_family;

    vk_result = vkCreateCommandPool(renderer->device, &command_pool_info, NULL,
                                    &renderer->frame_command_pools[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_command_pools;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkCommandBufferAllocateInfo command_buffer_info;

    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.pNext = NULL;
    command_buffer_info.commandPool = renderer->frame_command_pools[i];
    command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;

    vk_result =
        vkAllocateCommandBuffers(renderer->device, &command_buffer_info,
                                 &renderer->frame_command_buffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_command_buffers;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkFenceCreateInfo fence_info;

    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = NULL;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence(renderer->device, &fence_info, NULL,
                              &renderer->frame_in_flight_fences[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_in_flight_fences;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkSemaphoreCreateInfo semaphore_info;

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;

    vk_result = vkCreateSemaphore(renderer->device, &semaphore_info, NULL,
                                  &renderer->frame_acquire_semaphores[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_available_semaphores;
    }
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkSemaphoreCreateInfo semaphore_info;

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;

    vk_result = vkCreateSemaphore(renderer->device, &semaphore_info, NULL,
                                  &renderer->frame_render_done_semaphores[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_render_done_semaphores;
    }
  }

  goto out;

error_destroy_render_done_semaphores:
  for (i = i - 1; i >= 0; --i)
    vkDestroySemaphore(renderer->device,
                       renderer->frame_render_done_semaphores[i], NULL);

  i = renderer->num_frames;

error_destroy_image_available_semaphores:
  for (i = i - 1; i >= 0; --i)
    vkDestroySemaphore(renderer->device, renderer->frame_acquire_semaphores[i],
                       NULL);

  i = renderer->num_frames;

error_destroy_in_flight_fences:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFence(renderer->device, renderer->frame_in_flight_fences[i],
                   NULL);

  i = renderer->num_frames;

error_destroy_command_buffers:
  for (i = i - 1; i >= 0; --i)
    vkFreeCommandBuffers(renderer->device, renderer->frame_command_pools[i], 1,
                         &renderer->frame_command_buffers[i]);

  i = renderer->num_frames;

error_destroy_command_pools:
  for (i = i - 1; i >= 0; --i)
    vkDestroyCommandPool(renderer->device, renderer->frame_command_pools[i],
                         NULL);

out:
  return code;
}

owl_private void
owl_renderer_deinit_frames(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->num_frames; ++i)
    vkDestroySemaphore(renderer->device,
                       renderer->frame_render_done_semaphores[i], NULL);

  for (i = 0; i < renderer->num_frames; ++i)
    vkDestroySemaphore(renderer->device, renderer->frame_acquire_semaphores[i],
                       NULL);

  for (i = 0; i < renderer->num_frames; ++i)
    vkDestroyFence(renderer->device, renderer->frame_in_flight_fences[i],
                   NULL);

  for (i = 0; i < renderer->num_frames; ++i)
    vkFreeCommandBuffers(renderer->device, renderer->frame_command_pools[i], 1,
                         &renderer->frame_command_buffers[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    vkDestroyCommandPool(renderer->device, renderer->frame_command_pools[i],
                         NULL);
}

owl_private owl_code
owl_renderer_init_garbage(struct owl_renderer *renderer) {
  int i;
  owl_code code;

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    code = owl_vector_init(&renderer->garbage_buffers[i], 8, 0);
    if (code)
      goto error_buffers_deinit;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    code = owl_vector_init(&renderer->garbage_memories[i], 8, 0);
    if (code)
      goto error_memories_deinit;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    code = owl_vector_init(&renderer->garbage_sets[i], 8, 0);
    if (code)
      goto error_sets_deinit;
  }

  goto out;

error_sets_deinit:
  for (i = i - 1; i >= 0; --i)
    owl_vector_deinit(renderer->garbage_sets[i]);

  i = renderer->num_frames;

error_memories_deinit:
  for (i = i - 1; i >= 0; --i)
    owl_vector_deinit(renderer->garbage_memories[i]);

  i = renderer->num_frames;

error_buffers_deinit:
  for (i = i - 1; i >= 0; --i)
    owl_vector_deinit(renderer->garbage_buffers[i]);

out:
  return code;
}

owl_private void
owl_renderer_deinit_garbage(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->num_frames; ++i) {
    uint64_t j;
    owl_vector(VkDescriptorSet) sets = renderer->garbage_sets[i];

    for (j = 0; j < owl_vector_size(sets); ++j)
      vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                           &sets[j]);

    owl_vector_deinit(sets);
  }

  for (i = 0; i < renderer->num_frames; ++i) {
    uint64_t j;
    owl_vector(VkDeviceMemory) memories = renderer->garbage_memories[i];

    for (j = 0; j < owl_vector_size(memories); ++j)
      vkFreeMemory(renderer->device, memories[j], NULL);

    owl_vector_deinit(memories);
  }

  for (i = 0; i < renderer->num_frames; ++i) {
    uint64_t j;
    owl_vector(VkBuffer) buffers = renderer->garbage_buffers[i];

    for (j = 0; j < owl_vector_size(buffers); ++j)
      vkDestroyBuffer(renderer->device, buffers[j], NULL);

    owl_vector_deinit(buffers);
  }
}

owl_private owl_code
owl_renderer_init_samplers(struct owl_renderer *renderer) {
  VkSamplerCreateInfo sampler_info;
  VkResult vk_result = VK_SUCCESS;

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
  sampler_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;

  vk_result = vkCreateSampler(renderer->device, &sampler_info, NULL,
                              &renderer->linear_sampler);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_private void
owl_renderer_deinit_samplers(struct owl_renderer *renderer) {
  vkDestroySampler(renderer->device, renderer->linear_sampler, NULL);
}

owl_public owl_code
owl_renderer_init(struct owl_renderer *renderer,
                  struct owl_plataform *plataform) {
  uint32_t width;
  uint32_t height;
  owl_v3 eye;
  owl_v3 direction;
  owl_v3 up;
  owl_code code;
  float ratio;
  float const fov = owl_deg2rad(45.0F);
  float const near = 0.01;
  float const far = 10.0F;

  renderer->plataform = plataform;
  renderer->im_command_buffer = VK_NULL_HANDLE;
  renderer->skybox_loaded = 0;
  renderer->font_loaded = 0;
  renderer->upload_buffer_in_use = 0;
  renderer->num_frames = OWL_NUM_IN_FLIGHT_FRAMES;

  renderer->clear_values[0].color.float32[0] = 0.0F;
  renderer->clear_values[0].color.float32[1] = 0.0F;
  renderer->clear_values[0].color.float32[2] = 0.0F;
  renderer->clear_values[0].color.float32[3] = 1.0F;
  renderer->clear_values[1].depthStencil.depth = 1.0F;
  renderer->clear_values[1].depthStencil.stencil = 0.0F;

  owl_plataform_get_framebuffer_dimensions(plataform, &width, &height);
  renderer->width = width;
  renderer->height = height;

  ratio = (float)width / (float)height;

  owl_v3_set(eye, 0.0F, 0.0F, 5.0F);
  owl_v4_set(direction, 0.0F, 0.0F, 1.0F, 1.0F);
  owl_v3_set(up, 0.0F, 1.0F, 0.0F);

  owl_m4_perspective(fov, ratio, near, far, renderer->projection);
  owl_m4_look(eye, direction, up, renderer->view);

  code = owl_renderer_init_instance(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize instance!\n");
    goto out;
  }

  code = owl_renderer_init_surface(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize surface!\n");
    goto error_deinit_instance;
  }

  code = owl_renderer_init_device(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize device!\n");
    goto error_deinit_surface;
  }

  code = owl_renderer_clamp_dimensions(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to clamp dimensions!\n");
    goto error_deinit_device;
  }

  code = owl_renderer_init_attachments(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize attachments\n");
    goto error_deinit_device;
  }

  code = owl_renderer_init_render_passes(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize render passes!\n");
    goto error_deinit_attachments;
  }

  code = owl_renderer_init_swapchain(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize swapchain!\n");
    goto error_deinit_render_passes;
  }

  code = owl_renderer_init_pools(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize pools!\n");
    goto error_deinit_swapchain;
  }

  code = owl_renderer_init_shaders(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize shaders!\n");
    goto error_deinit_pools;
  }

  code = owl_renderer_init_layouts(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize layouts!\n");
    goto error_deinit_shaders;
  }

  code = owl_renderer_init_pipelines(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize pipelines!\n");
    goto error_deinit_layouts;
  }

  code = owl_renderer_init_upload_buffer(renderer, OWL_DEFAULT_BUFFER_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize upload heap!\n");
    goto error_deinit_pipelines;
  }

  code = owl_renderer_init_samplers(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize samplers!\n");
    goto error_deinit_upload_buffer;
  }

  code = owl_renderer_init_frames(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize frames!\n");
    goto error_deinit_samplers;
  }

  code = owl_renderer_init_render_buffers(renderer, OWL_DEFAULT_BUFFER_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize frame heap!\n");
    goto error_deinit_frames;
  }

  code = owl_renderer_init_garbage(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize garbage!\n");
    goto error_deinit_render_buffers;
  }

  goto out;

error_deinit_render_buffers:
  owl_renderer_deinit_render_buffers(renderer);

error_deinit_frames:
  owl_renderer_deinit_frames(renderer);

error_deinit_samplers:
  owl_renderer_deinit_samplers(renderer);

error_deinit_upload_buffer:
  owl_renderer_deinit_upload_buffer(renderer);

error_deinit_pipelines:
  owl_renderer_deinit_pipelines(renderer);

error_deinit_layouts:
  owl_renderer_deinit_layouts(renderer);

error_deinit_shaders:
  owl_renderer_deinit_shaders(renderer);

error_deinit_pools:
  owl_renderer_deinit_pools(renderer);

error_deinit_swapchain:
  owl_renderer_deinit_swapchain(renderer);

error_deinit_render_passes:
  owl_renderer_deinit_render_passes(renderer);

error_deinit_attachments:
  owl_renderer_deinit_attachments(renderer);

error_deinit_device:
  owl_renderer_deinit_device(renderer);

error_deinit_surface:
  owl_renderer_deinit_surface(renderer);

error_deinit_instance:
  owl_renderer_deinit_instance(renderer);

out:
  return code;
}

owl_public void
owl_renderer_deinit(struct owl_renderer *renderer) {
  vkDeviceWaitIdle(renderer->device);

  if (renderer->font_loaded)
    owl_renderer_unload_font(renderer);

  if (renderer->skybox_loaded)
    owl_renderer_unload_skybox(renderer);

  owl_renderer_deinit_garbage(renderer);
  owl_renderer_deinit_render_buffers(renderer);
  owl_renderer_deinit_frames(renderer);
  owl_renderer_deinit_samplers(renderer);
  owl_renderer_deinit_upload_buffer(renderer);
  owl_renderer_deinit_pipelines(renderer);
  owl_renderer_deinit_layouts(renderer);
  owl_renderer_deinit_shaders(renderer);
  owl_renderer_deinit_pools(renderer);
  owl_renderer_deinit_swapchain(renderer);
  owl_renderer_deinit_render_passes(renderer);
  owl_renderer_deinit_attachments(renderer);
  owl_renderer_deinit_device(renderer);
  owl_renderer_deinit_surface(renderer);
  owl_renderer_deinit_instance(renderer);
}

owl_public owl_code
owl_renderer_resize_swapchain(struct owl_renderer *renderer) {
  uint32_t width;
  uint32_t height;
  float ratio;
  float const fov = owl_deg2rad(45.0F);
  float const near = 0.01;
  float const far = 10.0F;
  VkResult vk_result;
  owl_code code;

  vk_result = vkDeviceWaitIdle(renderer->device);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  owl_plataform_get_framebuffer_dimensions(renderer->plataform, &width,
                                           &height);
  renderer->width = width;
  renderer->height = height;

  ratio = (float)width / (float)height;
  owl_m4_perspective(fov, ratio, near, far, renderer->projection);

  owl_renderer_deinit_pipelines(renderer);
  owl_renderer_deinit_swapchain(renderer);
  owl_renderer_deinit_attachments(renderer);

  code = owl_renderer_clamp_dimensions(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to clamp dimensions!\n");
    goto out;
  }

  code = owl_renderer_init_attachments(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize attachments!\n");
    goto out;
  }

  code = owl_renderer_init_swapchain(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize swapchain!\n");
    goto error_deinit_attachments;
  }

  code = owl_renderer_init_pipelines(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize pipelines!\n");
    goto error_deinit_swapchain;
  }

  goto out;

error_deinit_swapchain:
  owl_renderer_deinit_swapchain(renderer);

error_deinit_attachments:
  owl_renderer_deinit_attachments(renderer);

out:
  return code;
}

owl_private void
owl_renderer_collect_garbage(struct owl_renderer *renderer) {
  uint32_t i;

  owl_vector(VkBuffer) buffers = renderer->garbage_buffers[renderer->frame];
  owl_vector(VkDeviceMemory) memories =
      renderer->garbage_memories[renderer->frame];
  owl_vector(VkDescriptorSet) sets = renderer->garbage_sets[renderer->frame];

  for (i = 0; i < owl_vector_size(sets); ++i)
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool, 1,
                         &sets[i]);

  owl_vector_clear(sets);

  for (i = 0; i < owl_vector_size(memories); ++i)
    vkFreeMemory(renderer->device, memories[i], NULL);

  owl_vector_clear(memories);

  for (i = 0; i < owl_vector_size(buffers); ++i)
    vkDestroyBuffer(renderer->device, buffers[i], NULL);

  owl_vector_clear(buffers);
}

owl_private owl_code
owl_renderer_push_garbage(struct owl_renderer *renderer) {
  owl_code code;
  int i;

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkBuffer buffer = renderer->render_buffers[i];
    owl_vector(VkBuffer) *buffers = &renderer->garbage_buffers[i];

    code = owl_vector_push_back(buffers, buffer);
    if (code)
      goto error_pop_buffers;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDeviceMemory memory = renderer->render_buffer_memories[i];
    owl_vector(VkDeviceMemory) *memories = &renderer->garbage_memories[i];

    code = owl_vector_push_back(memories, memory);
    if (code)
      goto error_pop_memories;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSet set = renderer->render_buffer_pvm_sets[i];
    owl_vector(VkDescriptorSet) *sets = &renderer->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_pvm_sets;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSet set = renderer->render_buffer_model1_sets[i];
    owl_vector(VkDescriptorSet) *sets = &renderer->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_model1_sets;
  }

  for (i = 0; i < (int)renderer->num_frames; ++i) {
    VkDescriptorSet set = renderer->render_buffer_model2_sets[i];
    owl_vector(VkDescriptorSet) *sets = &renderer->garbage_sets[i];

    code = owl_vector_push_back(sets, set);
    if (code)
      goto error_pop_model2_sets;
  }

  goto out;

error_pop_model2_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  i = renderer->num_frames;

error_pop_model1_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  i = renderer->num_frames;

error_pop_pvm_sets:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  i = renderer->num_frames;

error_pop_memories:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(renderer->garbage_memories[i]);

  i = renderer->num_frames;

error_pop_buffers:
  for (i = i - 1; i >= 0; ++i)
    owl_vector_pop(renderer->garbage_buffers[i]);

out:
  return code;
}

owl_private owl_code
owl_renderer_pop_garbage(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->num_frames; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    owl_vector_pop(renderer->garbage_sets[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    owl_vector_pop(renderer->garbage_memories[i]);

  for (i = 0; i < renderer->num_frames; ++i)
    owl_vector_pop(renderer->garbage_buffers[i]);

  return OWL_OK;
}

owl_public void *
owl_renderer_frame_allocate(struct owl_renderer *renderer, uint64_t size,
                            struct owl_frame_allocation *alloc) {
  uint32_t const frame = renderer->frame;
  uint64_t const alignment = renderer->render_buffer_alignment;
  uint64_t required = size + renderer->render_buffer_offset;

  if (renderer->render_buffer_size < required) {
    owl_code code;
    uint64_t new_size;

    code = owl_renderer_push_garbage(renderer);
    if (code)
      return NULL;

    new_size = owl_alignu2(required * 2, alignment);

    code = owl_renderer_init_render_buffers(renderer, new_size);
    if (code) {
      owl_renderer_pop_garbage(renderer);
      return NULL;
    }

    required = size + renderer->render_buffer_offset;
  }

  alloc->offset32 = (uint32_t)renderer->render_buffer_offset;
  alloc->offset = renderer->render_buffer_offset;
  alloc->buffer = renderer->render_buffers[renderer->frame];
  alloc->pvm_set = renderer->render_buffer_pvm_sets[renderer->frame];
  alloc->model1_set = renderer->render_buffer_model1_sets[renderer->frame];
  alloc->model2_set = renderer->render_buffer_model2_sets[renderer->frame];

  renderer->render_buffer_offset = owl_alignu2(required, alignment);

  return &((uint8_t *)(renderer->render_buffer_data[frame]))[alloc->offset];
}

#define owl_is_swapchain_out_of_date(vk_result)                               \
  (VK_ERROR_OUT_OF_DATE_KHR == (vk_result) ||                                 \
   VK_SUBOPTIMAL_KHR == (vk_result) ||                                        \
   VK_ERROR_SURFACE_LOST_KHR == (vk_result))

owl_public owl_code
owl_renderer_acquire_image(struct owl_renderer *renderer) {
  owl_code code = OWL_OK;
  VkResult vk_result = VK_SUCCESS;

  uint64_t const timeout = (uint64_t)-1;
  uint32_t *const image = &renderer->image;
  uint32_t const frame = renderer->frame;
  VkSemaphore acquire_semaphore = renderer->frame_acquire_semaphores[frame];

  vk_result =
      vkAcquireNextImageKHR(renderer->device, renderer->swapchain, timeout,
                            acquire_semaphore, VK_NULL_HANDLE, image);
  if (!owl_is_swapchain_out_of_date(vk_result) || !vk_result)
    return OWL_OK;

  code = owl_renderer_resize_swapchain(renderer);
  if (code)
    return code;

  vk_result =
      vkAcquireNextImageKHR(renderer->device, renderer->swapchain, timeout,
                            acquire_semaphore, VK_NULL_HANDLE, image);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_public owl_code
owl_renderer_begin_frame(struct owl_renderer *renderer) {
  VkCommandBufferBeginInfo command_buffer_info;
  VkRenderPassBeginInfo pass_info;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  uint64_t const timeout = (uint64_t)-1;
  uint32_t const frame = renderer->frame;
  VkFence in_flight = renderer->frame_in_flight_fences[frame];
  VkCommandPool command_pool = renderer->frame_command_pools[frame];
  VkCommandBuffer command_buffer = renderer->frame_command_buffers[frame];

  code = owl_renderer_acquire_image(renderer);
  if (code)
    return code;

  vk_result =
      vkWaitForFences(renderer->device, 1, &in_flight, VK_TRUE, timeout);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vk_result = vkResetFences(renderer->device, 1, &in_flight);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vk_result = vkResetCommandPool(renderer->device, command_pool, 0);
  if (vk_result)
    return OWL_ERROR_FATAL;

  owl_renderer_collect_garbage(renderer);

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_info.pInheritanceInfo = NULL;

  vk_result = vkBeginCommandBuffer(command_buffer, &command_buffer_info);
  if (vk_result)
    return OWL_ERROR_FATAL;

  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  pass_info.pNext = NULL;
  pass_info.renderPass = renderer->main_render_pass;
  pass_info.framebuffer = renderer->framebuffers[renderer->image];
  pass_info.renderArea.offset.x = 0;
  pass_info.renderArea.offset.y = 0;
  pass_info.renderArea.extent.width = renderer->width;
  pass_info.renderArea.extent.height = renderer->height;
  pass_info.clearValueCount = owl_array_size(renderer->clear_values);
  pass_info.pClearValues = renderer->clear_values;

  vkCmdBeginRenderPass(command_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

  return OWL_OK;
}

owl_public owl_code
owl_renderer_end_frame(struct owl_renderer *renderer) {

  VkPipelineStageFlagBits stage;
  VkSubmitInfo submit_info;
  VkPresentInfoKHR present_info;

  uint32_t const frame = renderer->frame;
  VkCommandBuffer command_buffer = renderer->frame_command_buffers[frame];
  VkFence in_flight = renderer->frame_in_flight_fences[frame];
  VkSemaphore acquire_semaphore = renderer->frame_acquire_semaphores[frame];
  VkSemaphore render_done_semaphore =
      renderer->frame_render_done_semaphores[frame];

  VkResult vk_result;

  vkCmdEndRenderPass(command_buffer);
  vk_result = vkEndCommandBuffer(command_buffer);
  if (vk_result)
    return OWL_ERROR_FATAL;

  stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &acquire_semaphore;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_done_semaphore;
  submit_info.pWaitDstStageMask = &stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  vk_result =
      vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, in_flight);
  if (vk_result)
    return OWL_ERROR_FATAL;

  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_done_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &renderer->swapchain;
  present_info.pImageIndices = &renderer->image;
  present_info.pResults = NULL;

  vk_result = vkQueuePresentKHR(renderer->present_queue, &present_info);
  if (owl_is_swapchain_out_of_date(vk_result))
    return owl_renderer_resize_swapchain(renderer);

  if (renderer->num_frames == ++renderer->frame)
    renderer->frame = 0;

  renderer->render_buffer_offset = 0;

  return OWL_OK;
}

owl_public void *
owl_renderer_upload_allocate(struct owl_renderer *renderer, uint64_t size,
                             struct owl_upload_allocation *alloc) {
  if (renderer->upload_buffer_in_use)
    return NULL;

  if (renderer->upload_buffer_size < size) {
    owl_code code;

    owl_renderer_deinit_upload_buffer(renderer);

    code = owl_renderer_init_upload_buffer(renderer, size * 2);
    if (code)
      return NULL;
  }

  alloc->buffer = renderer->upload_buffer;
  renderer->upload_buffer_in_use = 1;

  return renderer->upload_buffer_data;
}

owl_public void
owl_renderer_upload_free(struct owl_renderer *renderer, void *ptr) {
  assert(renderer->upload_buffer_data == ptr);
  renderer->upload_buffer_in_use = 0;
}

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_HEIGHT * OWL_FONT_ATLAS_WIDTH)

owl_public owl_code
owl_renderer_load_font(struct owl_renderer *renderer, uint32_t size,
                       char const *path) {
  owl_code code;
  uint8_t *bitmap;
  stbtt_pack_context pack;
  struct owl_plataform_file file;
  struct owl_texture_2d_desc texture_desc;
  int stb_result;
  int const width = OWL_FONT_ATLAS_WIDTH;
  int const height = OWL_FONT_ATLAS_HEIGHT;
  int const first = OWL_FONT_FIRST_CHAR;
  int const num = OWL_FONT_NUM_CHARS;

  if (renderer->font_loaded)
    owl_renderer_unload_font(renderer);

  code = owl_plataform_load_file(path, &file);
  if (code)
    return code;

  bitmap = owl_calloc(OWL_FONT_ATLAS_SIZE, sizeof(uint8_t));
  if (!bitmap) {
    code = OWL_ERROR_NO_MEMORY;
    goto error_unload_file;
  }

  stb_result = stbtt_PackBegin(&pack, bitmap, width, height, 0, 1, NULL);
  if (!stb_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_bitmap;
  }

  stbtt_PackSetOversampling(&pack, 2, 2);

  stb_result =
      stbtt_PackFontRange(&pack, file.data, 0, size, first, num,
                          (stbtt_packedchar *)(&renderer->font_chars[0]));
  if (!stb_result) {
    code = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  texture_desc.source = OWL_TEXTURE_SOURCE_DATA;
  texture_desc.data = bitmap;
  texture_desc.width = OWL_FONT_ATLAS_WIDTH;
  texture_desc.height = OWL_FONT_ATLAS_HEIGHT;
  texture_desc.format = OWL_PIXEL_FORMAT_R8_UNORM;

  code = owl_texture_2d_init(&renderer->font_atlas, renderer, &texture_desc);
  if (!code)
    renderer->font_loaded = 1;

error_pack_end:
  stbtt_PackEnd(&pack);

error_free_bitmap:
  owl_free(bitmap);

error_unload_file:
  owl_plataform_unload_file(&file);

  return code;
}

owl_public void
owl_renderer_unload_font(struct owl_renderer *renderer) {
  owl_texture_2d_deinit(&renderer->font_atlas, renderer);
  renderer->font_loaded = 0;
}

#define OWL_MAX_SKYBOX_PATH_LENGTH 256

owl_public owl_code
owl_renderer_load_skybox(struct owl_renderer *renderer, char const *path) {
  int i;
  owl_code code;
  struct owl_texture_cube_desc texture_desc;
  char buffers[6][OWL_MAX_SKYBOX_PATH_LENGTH];

  owl_local_persist char const *names[6] = {"left.jpg",  "right.jpg",
                                            "top.jpg",   "bottom.jpg",
                                            "front.jpg", "back.jpg"};

  for (i = 0; i < (int)owl_array_size(names); ++i) {
    snprintf(buffers[i], OWL_MAX_SKYBOX_PATH_LENGTH, "%s/%s", path, names[i]);
    texture_desc.files[i] = buffers[i];
  }

  code = owl_texture_cube_init(&renderer->skybox, renderer, &texture_desc);
  if (!code)
    renderer->skybox_loaded = 1;

  return code;
}

owl_public void
owl_renderer_unload_skybox(struct owl_renderer *renderer) {
  owl_texture_cube_deinit(&renderer->skybox, renderer);
  renderer->skybox_loaded = 0;
}

owl_public uint32_t
owl_renderer_find_memory_type(struct owl_renderer *renderer, uint32_t filter,
                              uint32_t props) {
  uint32_t ty;
  VkPhysicalDeviceMemoryProperties memprops;

  vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &memprops);

  for (ty = 0; ty < memprops.memoryTypeCount; ++ty) {
    uint32_t cur = memprops.memoryTypes[ty].propertyFlags;

    if ((cur & props) && (filter & (1U << ty)))
      return ty;
  }

  return (uint32_t)-1;
}

owl_public owl_code
owl_renderer_begin_im_command_buffer(struct owl_renderer *renderer) {
  VkCommandBufferAllocateInfo command_buffer_info;
  VkCommandBufferBeginInfo begin_info;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  owl_assert(!renderer->im_command_buffer);

  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.pNext = NULL;
  command_buffer_info.commandPool = renderer->command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  command_buffer_info.commandBufferCount = 1;

  vk_result = vkAllocateCommandBuffers(renderer->device, &command_buffer_info,
                                       &renderer->im_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = NULL;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;

  vk_result = vkBeginCommandBuffer(renderer->im_command_buffer, &begin_info);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_im_command_buffer_deinit;
  }

  goto out;

error_im_command_buffer_deinit:
  vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1,
                       &renderer->im_command_buffer);

out:
  return code;
}

owl_public owl_code
owl_renderer_end_im_command_buffer(struct owl_renderer *renderer) {
  VkSubmitInfo submit_info;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  owl_assert(renderer->im_command_buffer);

  vk_result = vkEndCommandBuffer(renderer->im_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = NULL;
  submit_info.pWaitDstStageMask = NULL;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &renderer->im_command_buffer;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = NULL;

  vk_result = vkQueueSubmit(renderer->graphics_queue, 1, &submit_info, NULL);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  vk_result = vkQueueWaitIdle(renderer->graphics_queue);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  renderer->im_command_buffer = VK_NULL_HANDLE;

cleanup:
  vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1,
                       &renderer->im_command_buffer);

  return code;
}

owl_public owl_code
owl_renderer_fill_glyph(struct owl_renderer *renderer, char c, owl_v2 offset,
                        struct owl_glyph *glyph) {
  stbtt_aligned_quad quad;
  owl_code code = OWL_OK;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&renderer->font_chars[0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_FONT_FIRST_CHAR, &offset[0], &offset[1], &quad,
                      1);

  owl_v3_set(glyph->positions[0], quad.x0, quad.y0, 0.0F);
  owl_v3_set(glyph->positions[1], quad.x1, quad.y0, 0.0F);
  owl_v3_set(glyph->positions[2], quad.x0, quad.y1, 0.0F);
  owl_v3_set(glyph->positions[3], quad.x1, quad.y1, 0.0F);

  owl_v2_set(glyph->uvs[0], quad.s0, quad.t0);
  owl_v2_set(glyph->uvs[1], quad.s1, quad.t0);
  owl_v2_set(glyph->uvs[2], quad.s0, quad.t1);
  owl_v2_set(glyph->uvs[3], quad.s1, quad.t1);

  return code;
}

#include "owl_renderer.h"

#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_texture.h"
#include "owl_vector_math.h"
#include "stb_truetype.h"

#include <stdio.h>

#define OWL_DEFAULT_BUFFER_SIZE (1 << 16)

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32
owl_renderer_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                            VkDebugUtilsMessageTypeFlagsEXT type,
                            VkDebugUtilsMessengerCallbackDataEXT const *data,
                            void *user) {
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

OWL_PRIVATE char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif

OWL_PRIVATE owl_code
owl_renderer_init_instance(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;

  {
    owl_code code;

    uint32_t extension_count;
    char const *const *extensions;

    VkApplicationInfo application_info;
    VkInstanceCreateInfo instance_create_info;

    char const *name = owl_plataform_get_title(renderer->plataform);

    code = owl_plataform_get_required_instance_extensions(
        renderer->plataform, &extension_count, &extensions);
    if (code)
      return code;

    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pNext = NULL;
    application_info.pApplicationName = name;
    application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.pEngineName = "No Engine";
    application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    application_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pNext = NULL;
    instance_create_info.flags = 0;
    instance_create_info.pApplicationInfo = &application_info;
#if defined(OWL_ENABLE_VALIDATION)
    instance_create_info.enabledLayerCount = OWL_ARRAY_SIZE(
        debug_validation_layers);
    instance_create_info.ppEnabledLayerNames = debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
    instance_create_info.enabledLayerCount = 0;
    instance_create_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
    instance_create_info.enabledExtensionCount = extension_count;
    instance_create_info.ppEnabledExtensionNames = extensions;

    vk_result = vkCreateInstance(&instance_create_info, NULL,
                                 &renderer->instance);
    if (vk_result)
      goto error;
  }

#if defined(OWL_ENABLE_VALIDATION)
  {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info;

    renderer->vk_create_debug_utils_messenger_ext =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            renderer->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!renderer->vk_create_debug_utils_messenger_ext)
      goto error_destroy_instance;

    renderer->vk_destroy_debug_utils_messenger_ext =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            renderer->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (!renderer->vk_destroy_debug_utils_messenger_ext)
      goto error_destroy_instance;

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
    debug_messenger_create_info.pfnUserCallback = owl_renderer_debug_callback;
    debug_messenger_create_info.pUserData = renderer;

    vk_result = renderer->vk_create_debug_utils_messenger_ext(
        renderer->instance, &debug_messenger_create_info, NULL,
        &renderer->debug_messenger);
    if (vk_result)
      goto error_destroy_instance;
  }
#endif

  return OWL_OK;

#if defined(OWL_ENABLE_VALIDATION)
error_destroy_instance:
  vkDestroyInstance(renderer->instance, NULL);
#endif

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_instance(struct owl_renderer *renderer) {
#if defined(OWL_ENABLE_VALIDATION)
  renderer->vk_destroy_debug_utils_messenger_ext(
      renderer->instance, renderer->debug_messenger, NULL);
#endif
  vkDestroyInstance(renderer->instance, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_surface(struct owl_renderer *renderer) {
  return owl_plataform_create_vulkan_surface(renderer->plataform, renderer);
}

OWL_PRIVATE void
owl_renderer_deinit_surface(struct owl_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

OWL_PRIVATE char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

OWL_PRIVATE int
owl_renderer_validate_queue_families(struct owl_renderer *renderer) {
  if ((uint32_t)-1 == renderer->graphics_queue_family)
    return 0;

  if ((uint32_t)-1 == renderer->present_queue_family)
    return 0;

  return 1;
}

OWL_PRIVATE int
owl_renderer_ensure_queue_families(struct owl_renderer *renderer) {
  uint32_t i;
  VkResult vk_result = VK_SUCCESS;
  uint32_t property_count = 0;
  VkQueueFamilyProperties *properties = NULL;

  renderer->graphics_queue_family = (uint32_t)-1;
  renderer->present_queue_family = (uint32_t)-1;

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &property_count, NULL);

  properties = OWL_MALLOC(property_count * sizeof(*properties));
  if (!properties)
    return 0;

  vkGetPhysicalDeviceQueueFamilyProperties(renderer->physical_device,
                                           &property_count, properties);

  for (i = 0; i < property_count; ++i) {
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
  OWL_FREE(properties);

  return owl_renderer_validate_queue_families(renderer);
}

OWL_PRIVATE int
owl_renderer_validate_device_extensions(struct owl_renderer *renderer) {
  int32_t found = 0;
  uint32_t i = 0;
  uint32_t property_count = 0;
  VkExtensionProperties *properties = NULL;
  int32_t *state = NULL;
  VkResult vk_result = VK_SUCCESS;

  vk_result = vkEnumerateDeviceExtensionProperties(
      renderer->physical_device, NULL, &property_count, NULL);
  if (vk_result)
    return 0;

  properties = OWL_MALLOC(property_count * sizeof(*properties));
  if (!properties)
    return 0;

  vk_result = vkEnumerateDeviceExtensionProperties(
      renderer->physical_device, NULL, &property_count, properties);
  if (vk_result)
    goto out_free_properties;

  state = OWL_MALLOC(OWL_ARRAY_SIZE(device_extensions) * sizeof(*state));
  if (!state)
    goto out_free_state;

  for (i = 0; i < OWL_ARRAY_SIZE(device_extensions); ++i)
    state[i] = 0;

  for (i = 0; i < property_count; ++i) {
    uint32_t j;
    for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j) {
      size_t const max_extension_length = VK_MAX_EXTENSION_NAME_SIZE;
      char const *l = device_extensions[j];
      char const *r = properties[j].extensionName;

      uint32_t minlen = OWL_MIN(max_extension_length, OWL_STRLEN(l));

      state[j] = !OWL_STRNCMP(l, r, minlen);
    }
  }

  found = 1;
  for (i = 0; i < OWL_ARRAY_SIZE(device_extensions) && found; ++i)
    if (!state[i])
      found = 0;

out_free_state:
  OWL_FREE(state);

out_free_properties:
  OWL_FREE(properties);

  return found;
}

OWL_PRIVATE owl_code
owl_renderer_ensure_surface_format(struct owl_renderer *renderer,
                                   VkFormat format,
                                   VkColorSpaceKHR color_space) {
  uint32_t i;
  uint32_t format_count = 0;
  VkSurfaceFormatKHR *formats = NULL;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_ERROR_NOT_FOUND;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &format_count, NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  formats = OWL_MALLOC(format_count * sizeof(*formats));
  if (!formats)
    return OWL_ERROR_NO_MEMORY;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &format_count, formats);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out_free_formats;
  }

  for (i = 0; i < format_count && OWL_ERROR_NOT_FOUND == code; ++i)
    if (formats[i].format == format && formats[i].colorSpace == color_space)
      code = OWL_OK;

  if (!code) {
    renderer->surface_format.format = format;
    renderer->surface_format.colorSpace = color_space;
  }

out_free_formats:
  OWL_FREE(formats);

  return code;
}

OWL_PRIVATE owl_code
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

OWL_PRIVATE int32_t
owl_renderer_validate_physical_device(struct owl_renderer *renderer) {
  int32_t has_families;
  int32_t has_extensions;
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

  has_families = owl_renderer_ensure_queue_families(renderer);
  if (!has_families)
    return 0;

  has_extensions = owl_renderer_validate_device_extensions(renderer);
  if (!has_extensions)
    return 0;

  vkGetPhysicalDeviceFeatures(renderer->physical_device, &feats);
  if (!feats.samplerAnisotropy)
    return 0;

  return 1;
}

OWL_PRIVATE owl_code
owl_renderer_ensure_physical_device(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t device_count = 0;
  VkPhysicalDevice *devices = NULL;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count,
                                         NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  devices = OWL_MALLOC(device_count * sizeof(*devices));

  if (!devices)
    return OWL_ERROR_NO_MEMORY;

  vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count,
                                         devices);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out_free_devices;
  }

  for (i = 0; i < device_count; ++i) {
    renderer->physical_device = devices[i];

    if (owl_renderer_validate_physical_device(renderer))
      break;
  }

  if (!owl_renderer_validate_queue_families(renderer)) {
    code = OWL_ERROR_FATAL;
    goto out_free_devices;
  }

out_free_devices:
  OWL_FREE(devices);

  return code;
}

OWL_PRIVATE owl_code
owl_renderer_init_device(struct owl_renderer *renderer) {
  VkPhysicalDeviceFeatures device_features;
  VkDeviceCreateInfo device_create_info;
  VkDeviceQueueCreateInfo queue_create_info[2];
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

  queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info[0].pNext = NULL;
  queue_create_info[0].flags = 0;
  queue_create_info[0].queueFamilyIndex = renderer->graphics_queue_family;
  queue_create_info[0].queueCount = 1;
  queue_create_info[0].pQueuePriorities = &priority;

  queue_create_info[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info[1].pNext = NULL;
  queue_create_info[1].flags = 0;
  queue_create_info[1].queueFamilyIndex = renderer->present_queue_family;
  queue_create_info[1].queueCount = 1;
  queue_create_info[1].pQueuePriorities = &priority;

  vkGetPhysicalDeviceFeatures(renderer->physical_device, &device_features);

  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = NULL;
  device_create_info.flags = 0;
  if (renderer->graphics_queue_family == renderer->present_queue_family) {
    device_create_info.queueCreateInfoCount = 1;
  } else {
    device_create_info.queueCreateInfoCount = 2;
  }
  device_create_info.pQueueCreateInfos = queue_create_info;
  device_create_info.enabledLayerCount = 0;      /* deprecated */
  device_create_info.ppEnabledLayerNames = NULL; /* deprecated */
  device_create_info.enabledExtensionCount = OWL_ARRAY_SIZE(device_extensions);
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pEnabledFeatures = &device_features;

  vk_result = vkCreateDevice(renderer->physical_device, &device_create_info,
                             NULL, &renderer->device);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vkGetDeviceQueue(renderer->device, renderer->graphics_queue_family, 0,
                   &renderer->graphics_queue);

  vkGetDeviceQueue(renderer->device, renderer->present_queue_family, 0,
                   &renderer->present_queue);

  return code;
}

OWL_PRIVATE void
owl_renderer_deinit_device(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

OWL_PRIVATE owl_code
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

    renderer->width = OWL_CLAMP(renderer->width, min_width, max_width);
    renderer->height = OWL_CLAMP(renderer->height, min_height, max_height);
  }

  return OWL_OK;
}

OWL_PRIVATE owl_code
owl_renderer_init_attachments(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;

  {
    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = renderer->surface_format.format;
    image_create_info.extent.width = renderer->width;
    image_create_info.extent.height = renderer->height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = renderer->msaa;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &image_create_info, NULL,
                              &renderer->color_image);
    if (vk_result)
      goto error;
  }

  {
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetImageMemoryRequirements(renderer->device, renderer->color_image,
                                 &memory_requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->color_memory);
    if (vk_result)
      goto error_destroy_color_image;

    vk_result = vkBindImageMemory(renderer->device, renderer->color_image,
                                  renderer->color_memory, 0);
    if (vk_result)
      goto error_free_color_memory;
  }

  {
    VkImageViewCreateInfo image_view_create_info;

    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = renderer->color_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = renderer->surface_format.format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_view_create_info,
                                  NULL, &renderer->color_image_view);
    if (vk_result)
      goto error_free_color_memory;
  }

  {
    VkFormatProperties d24_unorm_s8_uint_properties;
    VkFormatProperties d32_sfloat_s8_uint_properties;

    vkGetPhysicalDeviceFormatProperties(renderer->physical_device,
                                        VK_FORMAT_D24_UNORM_S8_UINT,
                                        &d24_unorm_s8_uint_properties);

    vkGetPhysicalDeviceFormatProperties(renderer->physical_device,
                                        VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        &d32_sfloat_s8_uint_properties);

    if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
        d24_unorm_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    } else if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
               d32_sfloat_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    } else {
      goto error_destroy_color_image_view;
    }
  }

  {

    VkImageCreateInfo image_create_info;

    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.flags = 0;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = renderer->depth_format;
    image_create_info.extent.width = renderer->width;
    image_create_info.extent.height = renderer->height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = renderer->msaa;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = NULL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &image_create_info, NULL,
                              &renderer->depth_image);
    if (vk_result)
      goto error_destroy_color_image_view;
  }

  {
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetImageMemoryRequirements(renderer->device, renderer->depth_image,
                                 &memory_requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->depth_memory);
    if (vk_result)
      goto error_destroy_depth_image;

    vk_result = vkBindImageMemory(renderer->device, renderer->depth_image,
                                  renderer->depth_memory, 0);
    if (vk_result)
      goto error_free_depth_memory;
  }
  {
    VkImageViewCreateInfo image_view_create_info;

    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = renderer->depth_image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = renderer->depth_format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_view_create_info,
                                  NULL, &renderer->depth_image_view);
    if (vk_result)
      goto error_free_depth_memory;
  }

  return OWL_OK;

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

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_attachments(struct owl_renderer *renderer) {
  vkDestroyImageView(renderer->device, renderer->depth_image_view, NULL);
  vkFreeMemory(renderer->device, renderer->depth_memory, NULL);
  vkDestroyImage(renderer->device, renderer->depth_image, NULL);
  vkDestroyImageView(renderer->device, renderer->color_image_view, NULL);
  vkFreeMemory(renderer->device, renderer->color_memory, NULL);
  vkDestroyImage(renderer->device, renderer->color_image, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_render_passes(struct owl_renderer *renderer) {
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo render_pass_create_info;
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

  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.pNext = NULL;
  render_pass_create_info.flags = 0;
  render_pass_create_info.attachmentCount = OWL_ARRAY_SIZE(attachments);
  render_pass_create_info.pAttachments = attachments;
  render_pass_create_info.subpassCount = 1;
  render_pass_create_info.pSubpasses = &subpass;
  render_pass_create_info.dependencyCount = 1;
  render_pass_create_info.pDependencies = &dependency;

  vk_result = vkCreateRenderPass(renderer->device, &render_pass_create_info,
                                 NULL, &renderer->main_render_pass);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

OWL_PRIVATE void
owl_renderer_deinit_render_passes(struct owl_renderer *renderer) {
  vkDestroyRenderPass(renderer->device, renderer->main_render_pass, NULL);
}

#define OWL_MAX_PRESENT_MODE_COUNT 16

OWL_PRIVATE owl_code
owl_renderer_ensure_present_mode(struct owl_renderer *renderer,
                                 VkPresentModeKHR prefered_present_mode) {
  uint32_t i;

  uint32_t mode_count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODE_COUNT];

  int32_t found = 0;

  VkResult vk_result = VK_SUCCESS;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &mode_count, NULL);
  if (vk_result || OWL_MAX_PRESENT_MODE_COUNT <= mode_count)
    return OWL_ERROR_FATAL;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &mode_count, modes);
  if (vk_result)
    return OWL_ERROR_FATAL;

  for (i = 0; i < mode_count && !found; ++i)
    if (modes[i] == prefered_present_mode)
      found = 1;

  if (!found)
    return OWL_ERROR_FATAL;

  renderer->present_mode = prefered_present_mode;

  return OWL_OK;
}

OWL_PRIVATE owl_code
owl_renderer_init_swapchain(struct owl_renderer *renderer) {
  int32_t i;

  uint32_t families[2];
  VkSwapchainCreateInfoKHR swapchain_create_info;
  VkSurfaceCapabilitiesKHR surface_capabilities;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  code = owl_renderer_ensure_present_mode(renderer, VK_PRESENT_MODE_FIFO_KHR);
  if (code)
    return code;

  renderer->swapchain_image = 0;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      renderer->physical_device, renderer->surface, &surface_capabilities);

  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = NULL;
  swapchain_create_info.flags = 0;
  swapchain_create_info.surface = renderer->surface;
  swapchain_create_info.minImageCount = renderer->frame_count + 1;
  swapchain_create_info.imageFormat = renderer->surface_format.format;
  swapchain_create_info.imageColorSpace = renderer->surface_format.colorSpace;
  swapchain_create_info.imageExtent.width = renderer->width;
  swapchain_create_info.imageExtent.height = renderer->height;
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.preTransform = surface_capabilities.currentTransform;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = renderer->present_mode;
  swapchain_create_info.clipped = VK_TRUE;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

  families[0] = renderer->graphics_queue_family;
  families[1] = renderer->present_queue_family;

  if (renderer->graphics_queue_family == renderer->present_queue_family) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = NULL;
  } else {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = 2;
    swapchain_create_info.pQueueFamilyIndices = families;
  }

  vk_result = vkCreateSwapchainKHR(renderer->device, &swapchain_create_info,
                                   NULL, &renderer->swapchain);
  if (vk_result)
    goto error;

  vk_result = vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                      &renderer->swapchain_image_count, NULL);
  if (vk_result || OWL_RENDERER_MAX_SWAPCHAIN_IMAGE_COUNT <=
                       renderer->swapchain_image_count)
    goto error_destroy_swapchain;

  vk_result = vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                      &renderer->swapchain_image_count,
                                      renderer->swapchain_images);
  if (vk_result)
    goto error_destroy_swapchain;

  for (i = 0; i < (int32_t)renderer->swapchain_image_count; ++i) {
    VkImageViewCreateInfo image_view_create_info;

    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    image_view_create_info.image = renderer->swapchain_images[i];
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = renderer->surface_format.format;
    image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(renderer->device, &image_view_create_info,
                                  NULL, &renderer->swapchain_image_views[i]);
    if (vk_result)
      goto error_destroy_image_views;
  }

  for (i = 0; i < (int32_t)renderer->swapchain_image_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo framebuffer_create_info;

    attachments[0] = renderer->color_image_view;
    attachments[1] = renderer->depth_image_view;
    attachments[2] = renderer->swapchain_image_views[i];

    framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_create_info.pNext = NULL;
    framebuffer_create_info.flags = 0;
    framebuffer_create_info.renderPass = renderer->main_render_pass;
    framebuffer_create_info.attachmentCount = OWL_ARRAY_SIZE(attachments);
    framebuffer_create_info.pAttachments = attachments;
    framebuffer_create_info.width = renderer->width;
    framebuffer_create_info.height = renderer->height;
    framebuffer_create_info.layers = 1;

    vk_result = vkCreateFramebuffer(renderer->device, &framebuffer_create_info,
                                    NULL,
                                    &renderer->swapchain_framebuffers[i]);
    if (vk_result)
      goto error_destroy_framebuffers;
  }

  return OWL_OK;

error_destroy_framebuffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFramebuffer(renderer->device, renderer->swapchain_framebuffers[i],
                         NULL);

  i = renderer->swapchain_image_count;

error_destroy_image_views:
  for (i = i - 1; i >= 0; --i)
    vkDestroyImageView(renderer->device, renderer->swapchain_image_views[i],
                       NULL);

error_destroy_swapchain:
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);

error:
  return code;
}

OWL_PRIVATE void
owl_renderer_deinit_swapchain(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->swapchain_image_count; ++i)
    vkDestroyFramebuffer(renderer->device, renderer->swapchain_framebuffers[i],
                         NULL);

  for (i = 0; i < renderer->swapchain_image_count; ++i)
    vkDestroyImageView(renderer->device, renderer->swapchain_image_views[i],
                       NULL);

  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_pools(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;

  {
    VkCommandPoolCreateInfo command_pool_info;

    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    command_pool_info.queueFamilyIndex = renderer->graphics_queue_family;

    vk_result = vkCreateCommandPool(renderer->device, &command_pool_info, NULL,
                                    &renderer->command_pool);
    if (vk_result)
      goto error;
  }

  {
    VkDescriptorPoolSize sizes[6];
    VkDescriptorPoolCreateInfo descriptor_pool_create_info;
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

    descriptor_pool_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.pNext = NULL;
    descriptor_pool_create_info.flags =
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_create_info.maxSets = 256 * OWL_ARRAY_SIZE(sizes);
    descriptor_pool_create_info.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    descriptor_pool_create_info.pPoolSizes = sizes;

    vk_result = vkCreateDescriptorPool(renderer->device,
                                       &descriptor_pool_create_info, NULL,
                                       &renderer->descriptor_pool);
    if (vk_result)
      goto error_destroy_command_pool;
  }

  return OWL_OK;

error_destroy_command_pool:
  vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_pools(struct owl_renderer *renderer) {
  vkDestroyDescriptorPool(renderer->device, renderer->descriptor_pool, NULL);
  vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_shaders(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;

  {
    VkShaderModuleCreateInfo shader_create_info;

    /* TODO(samuel): Properly load code at runtime */
    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_basic.vert.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->basic_vertex_shader);
    if (vk_result)
      goto error;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_basic.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->basic_fragment_shader);
    if (vk_result)
      goto error_destroy_basic_vertex_shader;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_font.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->text_fragment_shader);

    if (vk_result)
      goto error_destroy_basic_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_model.vert.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->model_vertex_shader);

    if (VK_SUCCESS != vk_result)
      goto error_destroy_text_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_model.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->model_fragment_shader);

    if (vk_result)
      goto error_destroy_model_vertex_shader;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_skybox.vert.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->skybox_vertex_shader);

    if (vk_result)
      goto error_destroy_model_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo shader_create_info;

    OWL_LOCAL_PERSIST uint32_t const spv[] = {
#include "owl_skybox.frag.spv.u32"
    };

    shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_create_info.pNext = NULL;
    shader_create_info.flags = 0;
    shader_create_info.codeSize = sizeof(spv);
    shader_create_info.pCode = spv;

    vk_result = vkCreateShaderModule(renderer->device, &shader_create_info,
                                     NULL, &renderer->skybox_fragment_shader);

    if (vk_result)
      goto error_destroy_skybox_vertex_shader;
  }

  return OWL_OK;

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

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
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

OWL_PRIVATE owl_code
owl_renderer_init_layouts(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = NULL;
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.bindingCount = 1;
    descriptor_set_layout_create_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        renderer->device, &descriptor_set_layout_create_info, NULL,
        &renderer->ubo_vertex_descriptor_set_layout);
    if (vk_result)
      goto error;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = NULL;
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.bindingCount = 1;
    descriptor_set_layout_create_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        renderer->device, &descriptor_set_layout_create_info, NULL,
        &renderer->ubo_fragment_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_ubo_vertex_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                         VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = NULL;
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.bindingCount = 1;
    descriptor_set_layout_create_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        renderer->device, &descriptor_set_layout_create_info, NULL,
        &renderer->ubo_both_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_ubo_fragment_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_create_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_create_info.pNext = NULL;
    descriptor_set_layout_create_info.flags = 0;
    descriptor_set_layout_create_info.bindingCount = 1;
    descriptor_set_layout_create_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        renderer->device, &descriptor_set_layout_create_info, NULL,
        &renderer->ssbo_vertex_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_ubo_both_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;

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

    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.pNext = NULL;
    descriptor_set_layout_info.flags = 0;
    descriptor_set_layout_info.bindingCount = OWL_ARRAY_SIZE(bindings);
    descriptor_set_layout_info.pBindings = bindings;

    vk_result = vkCreateDescriptorSetLayout(
        renderer->device, &descriptor_set_layout_info, NULL,
        &renderer->image_fragment_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_ssbo_vertex_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayout layouts[2];
    VkPipelineLayoutCreateInfo pipeline_layout_create_info;

    layouts[0] = renderer->ubo_vertex_descriptor_set_layout;
    layouts[1] = renderer->image_fragment_descriptor_set_layout;

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = NULL;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
    pipeline_layout_create_info.pSetLayouts = layouts;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = NULL;

    vk_result = vkCreatePipelineLayout(renderer->device,
                                       &pipeline_layout_create_info, NULL,
                                       &renderer->common_pipeline_layout);
    if (vk_result)
      goto error_destroy_image_fragment_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayout layouts[6];
    VkPushConstantRange push_constant_range;
    VkPipelineLayoutCreateInfo pipeline_layout_create_info;

    push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(struct owl_model_material_push_constant);

    layouts[0] = renderer->ubo_both_descriptor_set_layout;
    layouts[1] = renderer->image_fragment_descriptor_set_layout;
    layouts[2] = renderer->image_fragment_descriptor_set_layout;
    layouts[3] = renderer->image_fragment_descriptor_set_layout;
    layouts[4] = renderer->ssbo_vertex_descriptor_set_layout;
    layouts[5] = renderer->ubo_fragment_descriptor_set_layout;

    pipeline_layout_create_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = NULL;
    pipeline_layout_create_info.flags = 0;
    pipeline_layout_create_info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
    pipeline_layout_create_info.pSetLayouts = layouts;
    pipeline_layout_create_info.pushConstantRangeCount = 1;
    pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;

    vk_result = vkCreatePipelineLayout(renderer->device,
                                       &pipeline_layout_create_info, NULL,
                                       &renderer->model_pipeline_layout);
    if (vk_result)
      goto error_destroy_common_pipeline_layout;
  }

  return OWL_OK;

error_destroy_common_pipeline_layout:
  vkDestroyPipelineLayout(renderer->device, renderer->common_pipeline_layout,
                          NULL);

error_destroy_image_fragment_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->image_fragment_descriptor_set_layout, NULL);

error_destroy_ssbo_vertex_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ssbo_vertex_descriptor_set_layout, NULL);

error_destroy_ubo_both_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_both_descriptor_set_layout, NULL);

error_destroy_ubo_fragment_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ubo_fragment_descriptor_set_layout, NULL);

error_destroy_ubo_vertex_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ubo_vertex_descriptor_set_layout, NULL);

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_layouts(struct owl_renderer *renderer) {
  vkDestroyPipelineLayout(renderer->device, renderer->model_pipeline_layout,
                          NULL);
  vkDestroyPipelineLayout(renderer->device, renderer->common_pipeline_layout,
                          NULL);
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->image_fragment_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ssbo_vertex_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(renderer->device,
                               renderer->ubo_both_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ubo_fragment_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      renderer->device, renderer->ubo_vertex_descriptor_set_layout, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_pipelines(struct owl_renderer *renderer) {
  VkVertexInputBindingDescription vertex_bindings;
  VkVertexInputAttributeDescription vertex_attributes[6];
  VkPipelineVertexInputStateCreateInfo vertex_input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterization;
  VkPipelineMultisampleStateCreateInfo multisample;
  VkPipelineColorBlendAttachmentState color_attachment;
  VkPipelineColorBlendStateCreateInfo color;
  VkPipelineDepthStencilStateCreateInfo depth;
  VkPipelineShaderStageCreateInfo stages[2];
  VkGraphicsPipelineCreateInfo pipeline_create_info;
  VkResult vk_result;

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
  vertex_input.vertexAttributeDescriptionCount = 3;
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

  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.pNext = NULL;
  viewport_state.flags = 0;
  viewport_state.viewportCount = 1;
  viewport_state.pViewports = &viewport;
  viewport_state.scissorCount = 1;
  viewport_state.pScissors = &scissor;

  rasterization.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization.pNext = NULL;
  rasterization.flags = 0;
  rasterization.depthClampEnable = VK_FALSE;
  rasterization.rasterizerDiscardEnable = VK_FALSE;
  rasterization.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization.depthBiasEnable = VK_FALSE;
  rasterization.depthBiasConstantFactor = 0.0F;
  rasterization.depthBiasClamp = 0.0F;
  rasterization.depthBiasSlopeFactor = 0.0F;
  rasterization.lineWidth = 1.0F;

  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
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
  color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                    VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT;

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
  depth.front.failOp = VK_STENCIL_OP_ZERO;
  depth.front.passOp = VK_STENCIL_OP_ZERO;
  depth.front.depthFailOp = VK_STENCIL_OP_ZERO;
  depth.front.compareOp = VK_COMPARE_OP_NEVER;
  depth.front.compareMask = 0;
  depth.front.writeMask = 0;
  depth.front.reference = 0;
  depth.back.failOp = VK_STENCIL_OP_ZERO;
  depth.back.passOp = VK_STENCIL_OP_ZERO;
  depth.back.depthFailOp = VK_STENCIL_OP_ZERO;
  depth.back.compareOp = VK_COMPARE_OP_NEVER;
  depth.back.compareMask = 0;
  depth.back.writeMask = 0;
  depth.back.reference = 0;
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

  pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_create_info.pNext = NULL;
  pipeline_create_info.flags = 0;
  pipeline_create_info.stageCount = OWL_ARRAY_SIZE(stages);
  pipeline_create_info.pStages = stages;
  pipeline_create_info.pVertexInputState = &vertex_input;
  pipeline_create_info.pInputAssemblyState = &input_assembly;
  pipeline_create_info.pTessellationState = NULL;
  pipeline_create_info.pViewportState = &viewport_state;
  pipeline_create_info.pRasterizationState = &rasterization;
  pipeline_create_info.pMultisampleState = &multisample;
  pipeline_create_info.pDepthStencilState = &depth;
  pipeline_create_info.pColorBlendState = &color;
  pipeline_create_info.pDynamicState = NULL;
  pipeline_create_info.layout = renderer->common_pipeline_layout;
  pipeline_create_info.renderPass = renderer->main_render_pass;
  pipeline_create_info.subpass = 0;
  pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_create_info.basePipelineIndex = -1;

  vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                        &pipeline_create_info, NULL,
                                        &renderer->basic_pipeline);
  if (vk_result)
    goto error;

  rasterization.polygonMode = VK_POLYGON_MODE_LINE;

  vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                        &pipeline_create_info, NULL,
                                        &renderer->wires_pipeline);
  if (vk_result)
    goto error_destroy_basic_pipeline;

  rasterization.polygonMode = VK_POLYGON_MODE_FILL;

  color_attachment.blendEnable = VK_TRUE;
  color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

  stages[1].module = renderer->text_fragment_shader;

  vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                        &pipeline_create_info, NULL,
                                        &renderer->text_pipeline);
  if (vk_result)
    goto error_destroy_wires_pipeline;

  vertex_bindings.stride = sizeof(struct owl_pnuujw_vertex);

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

  vertex_input.vertexAttributeDescriptionCount = 6;

  color_attachment.blendEnable = VK_FALSE;
  color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

  stages[0].module = renderer->model_vertex_shader;
  stages[1].module = renderer->model_fragment_shader;

  pipeline_create_info.layout = renderer->model_pipeline_layout;

  vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                        &pipeline_create_info, NULL,
                                        &renderer->model_pipeline);
  if (vk_result)
    goto error_destroy_text_pipeline;

  vertex_bindings.stride = sizeof(struct owl_p_vertex);

  vertex_attributes[0].binding = 0;
  vertex_attributes[0].location = 0;
  vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertex_attributes[0].offset = offsetof(struct owl_p_vertex, position);

  vertex_input.vertexAttributeDescriptionCount = 1;

  depth.depthTestEnable = VK_FALSE;
  depth.depthWriteEnable = VK_FALSE;

  stages[0].module = renderer->skybox_vertex_shader;
  stages[1].module = renderer->skybox_fragment_shader;

  pipeline_create_info.layout = renderer->common_pipeline_layout;

  vk_result = vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1,
                                        &pipeline_create_info, NULL,
                                        &renderer->skybox_pipeline);

  if (vk_result)
    goto error_destroy_model_pipeline;

  return OWL_OK;

error_destroy_model_pipeline:
  vkDestroyPipeline(renderer->device, renderer->model_pipeline, NULL);

error_destroy_text_pipeline:
  vkDestroyPipeline(renderer->device, renderer->text_pipeline, NULL);

error_destroy_wires_pipeline:
  vkDestroyPipeline(renderer->device, renderer->wires_pipeline, NULL);

error_destroy_basic_pipeline:
  vkDestroyPipeline(renderer->device, renderer->basic_pipeline, NULL);

error:
  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_pipelines(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device, renderer->skybox_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->model_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->text_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->wires_pipeline, NULL);
  vkDestroyPipeline(renderer->device, renderer->basic_pipeline, NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_upload_buffer(struct owl_renderer *renderer) {
  renderer->upload_buffer_in_use = 0;
  return OWL_OK;
}

OWL_PRIVATE void
owl_renderer_deinit_upload_buffer(struct owl_renderer *renderer) {
  if (renderer->upload_buffer_in_use) {
    vkFreeMemory(renderer->device, renderer->upload_buffer_memory, NULL);
    vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);
  }
}

OWL_PRIVATE owl_code
owl_renderer_init_garbage(struct owl_renderer *renderer) {
  uint32_t i;

  renderer->garbage = 0;

  for (i = 0; i < OWL_RENDERER_GARBAGE_COUNT; ++i) {
    renderer->garbage_buffer_counts[i] = 0;
    renderer->garbage_memory_counts[i] = 0;
    renderer->garbage_descriptor_set_counts[i] = 0;
  }

  return OWL_OK;
}

OWL_PRIVATE void
owl_renderer_collect_garbage(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const garbage_frames = OWL_RENDERER_GARBAGE_COUNT;
  uint32_t const collect = (renderer->garbage + 2) % garbage_frames;

  renderer->garbage = (renderer->garbage + 1) % garbage_frames;

  if (renderer->garbage_descriptor_set_counts[collect]) {
    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool,
                         renderer->garbage_descriptor_set_counts[collect],
                         renderer->garbage_descriptor_sets[collect]);
  }

  renderer->garbage_descriptor_set_counts[collect] = 0;

  for (i = 0; i < renderer->garbage_memory_counts[collect]; ++i) {
    VkDeviceMemory memory = renderer->garbage_memories[collect][i];
    vkFreeMemory(renderer->device, memory, NULL);
  }

  renderer->garbage_memory_counts[collect] = 0;

  for (i = 0; i < renderer->garbage_buffer_counts[collect]; ++i) {
    VkBuffer buffer = renderer->garbage_buffers[collect][i];
    vkDestroyBuffer(renderer->device, buffer, NULL);
  }

  renderer->garbage_buffer_counts[collect] = 0;
}

OWL_PRIVATE void
owl_renderer_deinit_garbage(struct owl_renderer *renderer) {
  renderer->garbage = 0;
  for (; renderer->garbage < OWL_RENDERER_GARBAGE_COUNT; ++renderer->garbage)
    owl_renderer_collect_garbage(renderer);
}

OWL_PRIVATE owl_code
owl_renderer_garbage_push_vertex(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const capacity = OWL_ARRAY_SIZE(renderer->garbage_buffers[0]);
  uint32_t const garbage = renderer->garbage;
  uint32_t const buffer_count = renderer->garbage_buffer_counts[garbage];
  uint32_t const memory_count = renderer->garbage_memory_counts[garbage];

  if (capacity <= buffer_count + renderer->frame_count)
    return OWL_ERROR_NO_SPACE;

  if (capacity <= memory_count + 1)
    return OWL_ERROR_NO_SPACE;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkBuffer buffer = renderer->vertex_buffers[i];
    renderer->garbage_buffers[garbage][buffer_count + i] = buffer;
  }

  renderer->garbage_buffer_counts[garbage] += renderer->frame_count;

  {
    VkDeviceMemory memory = renderer->vertex_buffer_memory;
    renderer->garbage_memories[garbage][memory_count] = memory;
  }

  renderer->garbage_memory_counts[garbage] += 1;

  return OWL_OK;
}

OWL_PRIVATE owl_code
owl_renderer_garbage_push_index(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const capacity = OWL_ARRAY_SIZE(renderer->garbage_buffers[0]);
  uint32_t const garbage = renderer->garbage;
  uint32_t const buffer_count = renderer->garbage_buffer_counts[garbage];
  uint32_t const memory_count = renderer->garbage_memory_counts[garbage];

  if (capacity <= buffer_count + renderer->frame_count)
    return OWL_ERROR_NO_SPACE;

  if (capacity <= memory_count + 1)
    return OWL_ERROR_NO_SPACE;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkBuffer buffer = renderer->index_buffers[i];
    renderer->garbage_buffers[garbage][buffer_count + i] = buffer;
  }

  renderer->garbage_buffer_counts[garbage] += renderer->frame_count;

  {
    VkDeviceMemory memory = renderer->index_buffer_memory;
    renderer->garbage_memories[garbage][memory_count] = memory;
  }

  renderer->garbage_memory_counts[garbage] += 1;

  return OWL_OK;
}

OWL_PRIVATE owl_code
owl_renderer_garbage_push_uniform(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const capacity = OWL_ARRAY_SIZE(renderer->garbage_buffers[0]);
  uint32_t const garbage = renderer->garbage;
  uint32_t const buffer_count = renderer->garbage_buffer_counts[garbage];
  uint32_t const memory_count = renderer->garbage_memory_counts[garbage];
  /* clang-format off */
  uint32_t descriptor_set_count = renderer->garbage_descriptor_set_counts[garbage];
  /* clang-format on */

  if (capacity <= buffer_count + renderer->frame_count)
    return OWL_ERROR_NO_SPACE;

  if (capacity <= memory_count + 1)
    return OWL_ERROR_NO_SPACE;

  /* clang-format off */
  if (capacity <= descriptor_set_count + renderer->frame_count * 3)
    return OWL_ERROR_NO_SPACE;
  /* clang-format on */

  for (i = 0; i < renderer->frame_count; ++i) {
    VkBuffer buffer = renderer->uniform_buffers[i];
    renderer->garbage_buffers[garbage][buffer_count + i] = buffer;
  }
  renderer->garbage_buffer_counts[garbage] += renderer->frame_count;

  {
    VkDeviceMemory memory = renderer->uniform_buffer_memory;
    renderer->garbage_memories[garbage][memory_count] = memory;
  }
  renderer->garbage_memory_counts[garbage] += 1;

  for (i = 0; i < renderer->frame_count; ++i) {
    uint32_t const count = descriptor_set_count;
    VkDescriptorSet descriptor_set;

    descriptor_set = renderer->uniform_pvm_descriptor_sets[i];
    renderer->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
  }
  descriptor_set_count += renderer->frame_count;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    uint32_t const count = descriptor_set_count;
    VkDescriptorSet descriptor_set;

    descriptor_set = renderer->uniform_model1_descriptor_sets[i];
    renderer->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
  }
  descriptor_set_count += renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    uint32_t const count = descriptor_set_count;
    VkDescriptorSet descriptor_set;

    descriptor_set = renderer->uniform_model2_descriptor_sets[i];
    renderer->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
  }
  descriptor_set_count += renderer->frame_count;
  renderer->garbage_descriptor_set_counts[garbage] = descriptor_set_count;

  return OWL_OK;
}

OWL_PRIVATE owl_code
owl_renderer_init_vertex_buffer(struct owl_renderer *renderer, uint64_t size) {
  int32_t i;

  VkResult vk_result = VK_SUCCESS;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                               &renderer->vertex_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryAllocateInfo memory_allocate_info;
    VkMemoryRequirements memory_requirements;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(
        renderer->device, renderer->vertex_buffers[0], &memory_requirements);

    renderer->vertex_buffer_alignment = memory_requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, memory_requirements.alignment);
    renderer->vertex_buffer_aligned_size = aligned_size;

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = aligned_size * renderer->frame_count;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->vertex_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->vertex_buffer_memory;
      VkBuffer buffer = renderer->vertex_buffers[j];
      vk_result = vkBindBufferMemory(renderer->device, buffer, memory,
                                     aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(renderer->device, renderer->vertex_buffer_memory,
                            0, size, 0, &renderer->vertex_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  renderer->vertex_buffer_offset = 0;
  renderer->vertex_buffer_size = size;

  return OWL_OK;

error_free_memory:
  vkFreeMemory(renderer->device, renderer->vertex_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(renderer->device, renderer->vertex_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_vertex_buffer(struct owl_renderer *renderer) {
  uint32_t i;
  vkFreeMemory(renderer->device, renderer->vertex_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(renderer->device, renderer->vertex_buffers[i], NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_index_buffer(struct owl_renderer *renderer, uint64_t size) {
  int32_t i;

  VkResult vk_result = VK_SUCCESS;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                               &renderer->index_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryAllocateInfo memory_allocate_info;
    VkMemoryRequirements memory_requirements;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(renderer->device, renderer->index_buffers[0],
                                  &memory_requirements);

    renderer->index_buffer_alignment = memory_requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, memory_requirements.alignment);
    renderer->index_buffer_aligned_size = aligned_size;

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = aligned_size * renderer->frame_count;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->index_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->index_buffer_memory;
      VkBuffer buffer = renderer->index_buffers[j];
      vk_result = vkBindBufferMemory(renderer->device, buffer, memory,
                                     aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(renderer->device, renderer->index_buffer_memory, 0,
                            size, 0, &renderer->index_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  renderer->index_buffer_offset = 0;
  renderer->index_buffer_size = size;

  return OWL_OK;

error_free_memory:
  vkFreeMemory(renderer->device, renderer->index_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(renderer->device, renderer->index_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_index_buffer(struct owl_renderer *renderer) {
  uint32_t i;

  vkFreeMemory(renderer->device, renderer->index_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(renderer->device, renderer->index_buffers[i], NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_uniform_buffer(struct owl_renderer *renderer,
                                 uint64_t size) {
  int32_t i;

  VkResult vk_result = VK_SUCCESS;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                               &renderer->uniform_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryAllocateInfo memory_allocate_info;
    VkMemoryRequirements memory_requirements;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(
        renderer->device, renderer->uniform_buffers[0], &memory_requirements);

    renderer->uniform_buffer_alignment = memory_requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, memory_requirements.alignment);
    renderer->uniform_buffer_aligned_size = aligned_size;

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = aligned_size * renderer->frame_count;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->uniform_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->uniform_buffer_memory;
      VkBuffer buffer = renderer->uniform_buffers[j];
      vk_result = vkBindBufferMemory(renderer->device, buffer, memory,
                                     aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(renderer->device, renderer->uniform_buffer_memory,
                            0, size, 0, &renderer->uniform_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    {
      VkDescriptorSetLayout layouts[3];
      VkDescriptorSet descriptor_sets[3];
      VkDescriptorSetAllocateInfo descriptor_set_allocate_info;

      layouts[0] = renderer->ubo_vertex_descriptor_set_layout;
      layouts[1] = renderer->ubo_both_descriptor_set_layout;
      layouts[2] = renderer->ubo_fragment_descriptor_set_layout;

      descriptor_set_allocate_info.sType =
          VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      descriptor_set_allocate_info.pNext = NULL;
      descriptor_set_allocate_info.descriptorPool = renderer->descriptor_pool;
      descriptor_set_allocate_info.descriptorSetCount = OWL_ARRAY_SIZE(
          layouts);
      descriptor_set_allocate_info.pSetLayouts = layouts;

      vk_result = vkAllocateDescriptorSets(
          renderer->device, &descriptor_set_allocate_info, descriptor_sets);
      if (vk_result)
        goto error_free_descriptor_sets;

      renderer->uniform_pvm_descriptor_sets[i] = descriptor_sets[0];
      renderer->uniform_model1_descriptor_sets[i] = descriptor_sets[1];
      renderer->uniform_model2_descriptor_sets[i] = descriptor_sets[2];
    }

    {
      VkDescriptorBufferInfo descriptors[3];
      VkWriteDescriptorSet writes[3];

      descriptors[0].buffer = renderer->uniform_buffers[i];
      descriptors[0].offset = 0;
      descriptors[0].range = sizeof(struct owl_pvm_uniform);

      writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[0].pNext = NULL;
      writes[0].dstSet = renderer->uniform_pvm_descriptor_sets[i];
      writes[0].dstBinding = 0;
      writes[0].dstArrayElement = 0;
      writes[0].descriptorCount = 1;
      writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      writes[0].pImageInfo = NULL;
      writes[0].pBufferInfo = &descriptors[0];
      writes[0].pTexelBufferView = NULL;

      descriptors[1].buffer = renderer->uniform_buffers[i];
      descriptors[1].offset = 0;
      descriptors[1].range = sizeof(struct owl_model_ubo1);

      writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[1].pNext = NULL;
      writes[1].dstSet = renderer->uniform_model1_descriptor_sets[i];
      writes[1].dstBinding = 0;
      writes[1].dstArrayElement = 0;
      writes[1].descriptorCount = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      writes[1].pImageInfo = NULL;
      writes[1].pBufferInfo = &descriptors[1];
      writes[1].pTexelBufferView = NULL;

      descriptors[2].buffer = renderer->uniform_buffers[i];
      descriptors[2].offset = 0;
      descriptors[2].range = sizeof(struct owl_model_ubo2);

      writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[2].pNext = NULL;
      writes[2].dstSet = renderer->uniform_model2_descriptor_sets[i];
      writes[2].dstBinding = 0;
      writes[2].dstArrayElement = 0;
      writes[2].descriptorCount = 1;
      writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      writes[2].pImageInfo = NULL;
      writes[2].pBufferInfo = &descriptors[2];
      writes[2].pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device, OWL_ARRAY_SIZE(writes), writes,
                             0, NULL);
    }
  }

  renderer->uniform_buffer_offset = 0;
  renderer->uniform_buffer_size = size;

  return OWL_OK;

error_free_descriptor_sets:
  for (i = i - 1; i >= 0; --i) {
    VkDescriptorSet descriptor_sets[3];

    descriptor_sets[0] = renderer->uniform_pvm_descriptor_sets[i];
    descriptor_sets[1] = renderer->uniform_model1_descriptor_sets[i];
    descriptor_sets[2] = renderer->uniform_model2_descriptor_sets[i];

    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool,
                         OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
  }

  i = renderer->frame_count;

error_free_memory:
  vkFreeMemory(renderer->device, renderer->uniform_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(renderer->device, renderer->uniform_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_uniform_buffer(struct owl_renderer *renderer) {
  uint32_t i;
  for (i = 0; i < renderer->frame_count; ++i) {
    VkDescriptorSet descriptor_sets[3];

    descriptor_sets[0] = renderer->uniform_pvm_descriptor_sets[i];
    descriptor_sets[1] = renderer->uniform_model1_descriptor_sets[i];
    descriptor_sets[2] = renderer->uniform_model2_descriptor_sets[i];

    vkFreeDescriptorSets(renderer->device, renderer->descriptor_pool,
                         OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
  }

  vkFreeMemory(renderer->device, renderer->uniform_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(renderer->device, renderer->uniform_buffers[i], NULL);
}

OWL_PRIVATE owl_code
owl_renderer_init_frames(struct owl_renderer *renderer) {
  int32_t i;
  VkResult vk_result = VK_SUCCESS;

  renderer->frame = 0;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkCommandPoolCreateInfo command_pool_create_info;

    command_pool_create_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.pNext = NULL;
    command_pool_create_info.flags = 0;
    command_pool_create_info.queueFamilyIndex =
        renderer->graphics_queue_family;

    vk_result = vkCreateCommandPool(renderer->device,
                                    &command_pool_create_info, NULL,
                                    &renderer->submit_command_pools[i]);
    if (vk_result)
      goto error_destroy_submit_command_pools;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkCommandBufferAllocateInfo command_buffer_allocate_info;
    VkCommandPool command_pool = renderer->submit_command_pools[i];

    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(renderer->device,
                                         &command_buffer_allocate_info,
                                         &renderer->submit_command_buffers[i]);
    if (vk_result)
      goto error_free_submit_command_buffers;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkFenceCreateInfo fence_create_info;

    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = NULL;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence(renderer->device, &fence_create_info, NULL,
                              &renderer->in_flight_fences[i]);
    if (vk_result)
      goto error_destroy_in_flight_fences;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkSemaphoreCreateInfo semaphore_create_info;

    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = NULL;
    semaphore_create_info.flags = 0;

    vk_result = vkCreateSemaphore(renderer->device, &semaphore_create_info,
                                  NULL, &renderer->acquire_semaphores[i]);
    if (vk_result)
      goto error_destroy_acquire_semaphores;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkSemaphoreCreateInfo semaphore_create_info;

    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_create_info.pNext = NULL;
    semaphore_create_info.flags = 0;

    vk_result = vkCreateSemaphore(renderer->device, &semaphore_create_info,
                                  NULL, &renderer->render_done_semaphores[i]);
    if (vk_result)
      goto error_destroy_render_done_semaphores;
  }

  return OWL_OK;

error_destroy_render_done_semaphores:
  for (i = i - 1; i > 0; --i) {
    VkSemaphore semaphore = renderer->render_done_semaphores[i];
    vkDestroySemaphore(renderer->device, semaphore, NULL);
  }

  i = renderer->frame_count;

error_destroy_acquire_semaphores:
  for (i = i - 1; i > 0; --i) {
    VkSemaphore semaphore = renderer->acquire_semaphores[i];
    vkDestroySemaphore(renderer->device, semaphore, NULL);
  }

  i = renderer->frame_count;

error_destroy_in_flight_fences:
  for (i = i - 1; i > 0; --i) {
    VkFence fence = renderer->in_flight_fences[i];
    vkDestroyFence(renderer->device, fence, NULL);
  }

  i = renderer->frame_count;

error_free_submit_command_buffers:
  for (i = i - 1; i > 0; --i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    VkCommandBuffer command_buffer = renderer->submit_command_buffers[i];
    vkFreeCommandBuffers(renderer->device, command_pool, 1, &command_buffer);
  }

error_destroy_submit_command_pools:
  for (i = i - 1; i > 0; --i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    vkDestroyCommandPool(renderer->device, command_pool, NULL);
  }

  return OWL_ERROR_FATAL;
}

OWL_PRIVATE void
owl_renderer_deinit_frames(struct owl_renderer *renderer) {
  uint32_t i;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkSemaphore semaphore = renderer->render_done_semaphores[i];
    vkDestroySemaphore(renderer->device, semaphore, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkSemaphore semaphore = renderer->acquire_semaphores[i];
    vkDestroySemaphore(renderer->device, semaphore, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkFence fence = renderer->in_flight_fences[i];
    vkDestroyFence(renderer->device, fence, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    VkCommandBuffer command_buffer = renderer->submit_command_buffers[i];
    vkFreeCommandBuffers(renderer->device, command_pool, 1, &command_buffer);
  }

  for (i = 0; i < renderer->frame_count; ++i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    vkDestroyCommandPool(renderer->device, command_pool, NULL);
  }
}

OWL_PRIVATE owl_code
owl_renderer_init_samplers(struct owl_renderer *renderer) {
  VkSamplerCreateInfo sampler_create_info;
  VkResult vk_result = VK_SUCCESS;

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
  sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;
  sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_create_info.unnormalizedCoordinates = VK_FALSE;

  vk_result = vkCreateSampler(renderer->device, &sampler_create_info, NULL,
                              &renderer->linear_sampler);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

OWL_PRIVATE void
owl_renderer_deinit_samplers(struct owl_renderer *renderer) {
  vkDestroySampler(renderer->device, renderer->linear_sampler, NULL);
}

OWL_PUBLIC owl_code
owl_renderer_init(struct owl_renderer *renderer,
                  struct owl_plataform *plataform) {
  uint32_t width;
  uint32_t height;
  owl_v3 eye;
  owl_v3 direction;
  owl_v3 up;
  owl_code code;
  float ratio;
  float const fov = OWL_DEGREES_AS_RADIANS(45.0F);
  float const near = 0.01;
  float const far = 10.0F;

  renderer->plataform = plataform;
  renderer->immediate_command_buffer = VK_NULL_HANDLE;
  renderer->skybox_loaded = 0;
  renderer->font_loaded = 0;
  renderer->frame_count = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

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

  OWL_V3_SET(eye, 0.0F, 0.0F, 5.0F);
  OWL_V4_SET(direction, 0.0F, 0.0F, 1.0F, 1.0F);
  OWL_V3_SET(up, 0.0F, 1.0F, 0.0F);

  owl_m4_perspective(fov, ratio, near, far, renderer->projection);
  owl_m4_look(eye, direction, up, renderer->view);

  code = owl_renderer_init_instance(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize instance!\n");
    goto error;
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

  code = owl_renderer_init_upload_buffer(renderer);
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

  code = owl_renderer_init_garbage(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize garbage!\n");
    goto error_deinit_frames;
  }

  code = owl_renderer_init_vertex_buffer(renderer, 1 << 12);
  if (code) {
    OWL_DEBUG_LOG("Filed to initilize vertex buffer!\n");
    goto error_deinit_garbage;
  }

  code = owl_renderer_init_index_buffer(renderer, 1 << 12);
  if (code) {
    OWL_DEBUG_LOG("Filed to initilize index buffer!\n");
    goto error_deinit_vertex_buffer;
  }

  code = owl_renderer_init_uniform_buffer(renderer, 1 << 12);
  if (code) {
    OWL_DEBUG_LOG("Filed to initilize uniform buffer!\n");
    goto error_deinit_index_buffer;
  }

  return OWL_OK;

error_deinit_index_buffer:
  owl_renderer_deinit_index_buffer(renderer);

error_deinit_vertex_buffer:
  owl_renderer_deinit_vertex_buffer(renderer);

error_deinit_garbage:
  owl_renderer_deinit_garbage(renderer);

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

error:
  return code;
}

OWL_PUBLIC void
owl_renderer_deinit(struct owl_renderer *renderer) {
  vkDeviceWaitIdle(renderer->device);

  if (renderer->font_loaded)
    owl_renderer_unload_font(renderer);

  if (renderer->skybox_loaded)
    owl_renderer_unload_skybox(renderer);

  owl_renderer_deinit_uniform_buffer(renderer);
  owl_renderer_deinit_index_buffer(renderer);
  owl_renderer_deinit_vertex_buffer(renderer);
  owl_renderer_deinit_garbage(renderer);
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

OWL_PUBLIC owl_code
owl_renderer_resize_swapchain(struct owl_renderer *renderer) {
  uint32_t width;
  uint32_t height;
  float ratio;
  float const fov = OWL_DEGREES_AS_RADIANS(45.0F);
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

OWL_PUBLIC void *
owl_renderer_vertex_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_vertex_allocation *allocation) {
  uint8_t *data = renderer->vertex_buffer_data;
  uint32_t const frame = renderer->frame;
  uint64_t const alignment = renderer->vertex_buffer_alignment;
  uint64_t required = size + renderer->vertex_buffer_offset;
  uint64_t aligned_size = renderer->vertex_buffer_aligned_size;

  if (renderer->vertex_buffer_size < required) {
    owl_code code;

    code = owl_renderer_garbage_push_vertex(renderer);
    if (code)
      return NULL;

    /* FIXME(samuel): ensure vertex buffer is valid after failure */
    code = owl_renderer_init_vertex_buffer(renderer, required);
    if (code)
      return NULL;

    data = renderer->vertex_buffer_data;
    required = size + renderer->vertex_buffer_offset;
    aligned_size = renderer->vertex_buffer_aligned_size;
  }

  allocation->offset = renderer->vertex_buffer_offset;
  allocation->buffer = renderer->vertex_buffers[frame];

  renderer->vertex_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

  return &data[frame * aligned_size + allocation->offset];
}

OWL_PUBLIC void *
owl_renderer_index_allocate(struct owl_renderer *renderer, uint64_t size,
                            struct owl_renderer_index_allocation *allocation) {
  uint8_t *data = renderer->index_buffer_data;
  uint32_t const frame = renderer->frame;
  uint64_t const alignment = renderer->index_buffer_alignment;
  uint64_t required = size + renderer->index_buffer_offset;
  uint64_t aligned_size = renderer->index_buffer_aligned_size;

  if (renderer->index_buffer_size < required) {
    owl_code code;

    code = owl_renderer_garbage_push_index(renderer);
    if (code)
      return NULL;

    /* FIXME(samuel): ensure index buffer is valid after failure */
    code = owl_renderer_init_index_buffer(renderer, required * 2);
    if (code)
      return NULL;

    data = renderer->index_buffer_data;
    required = size + renderer->index_buffer_offset;
    aligned_size = renderer->index_buffer_aligned_size;
  }

  allocation->offset = renderer->index_buffer_offset;
  allocation->buffer = renderer->index_buffers[frame];

  renderer->index_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

  return &data[frame * aligned_size + allocation->offset];
}

OWL_PUBLIC void *
owl_renderer_uniform_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_uniform_allocation *allocation) {
  uint8_t *data = renderer->uniform_buffer_data;
  uint32_t const frame = renderer->frame;
  uint64_t const alignment = renderer->uniform_buffer_alignment;
  uint64_t required = size + renderer->uniform_buffer_offset;
  uint64_t aligned_size = renderer->uniform_buffer_aligned_size;

  if (renderer->uniform_buffer_size < required) {
    owl_code code;

    code = owl_renderer_garbage_push_uniform(renderer);
    if (code)
      return NULL;

    /* FIXME(samuel): ensure uniform buffer is valid after failure */
    code = owl_renderer_init_uniform_buffer(renderer, required * 2);
    if (code)
      return NULL;

    data = renderer->uniform_buffer_data;
    required = size + renderer->uniform_buffer_offset;
    aligned_size = renderer->uniform_buffer_aligned_size;
  }

  allocation->offset = renderer->uniform_buffer_offset;
  allocation->buffer = renderer->uniform_buffers[frame];
  allocation->pvm_descriptor_set =
      renderer->uniform_pvm_descriptor_sets[frame];
  allocation->model1_descriptor_set =
      renderer->uniform_model1_descriptor_sets[frame];
  allocation->model2_descriptor_set =
      renderer->uniform_model2_descriptor_sets[frame];

  renderer->uniform_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

  return &data[frame * aligned_size + allocation->offset];
}

#define OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result)                      \
  (VK_ERROR_OUT_OF_DATE_KHR == (vk_result) ||                                 \
   VK_SUBOPTIMAL_KHR == (vk_result) ||                                        \
   VK_ERROR_SURFACE_LOST_KHR == (vk_result))

OWL_PUBLIC owl_code
owl_renderer_begin_frame(struct owl_renderer *renderer) {
  VkFramebuffer framebuffer;
  VkRenderPassBeginInfo render_pass_begin_info;
  VkCommandBufferBeginInfo command_buffer_begin_info;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  uint64_t const timeout = (uint64_t)-1;
  uint32_t const clear_value_count = OWL_ARRAY_SIZE(renderer->clear_values);
  uint32_t const frame = renderer->frame;
  VkClearValue const *clear_values = renderer->clear_values;
  VkCommandPool command_pool = renderer->submit_command_pools[frame];
  VkCommandBuffer command_buffer = renderer->submit_command_buffers[frame];
  VkFence in_flight_fence = renderer->in_flight_fences[frame];
  VkSemaphore acquire_semaphore = renderer->acquire_semaphores[frame];

  vk_result = vkAcquireNextImageKHR(renderer->device, renderer->swapchain,
                                    timeout, acquire_semaphore, VK_NULL_HANDLE,
                                    &renderer->swapchain_image);
  if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result)) {
    code = owl_renderer_resize_swapchain(renderer);
    if (code)
      return code;

    vk_result = vkAcquireNextImageKHR(
        renderer->device, renderer->swapchain, timeout, acquire_semaphore,
        VK_NULL_HANDLE, &renderer->swapchain_image);
    if (vk_result)
      return OWL_ERROR_FATAL;
  }

  vk_result = vkWaitForFences(renderer->device, 1, &in_flight_fence, VK_TRUE,
                              timeout);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vk_result = vkResetFences(renderer->device, 1, &in_flight_fence);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vk_result = vkResetCommandPool(renderer->device, command_pool, 0);
  if (vk_result)
    return OWL_ERROR_FATAL;

  owl_renderer_collect_garbage(renderer);

  command_buffer_begin_info.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = NULL;
  command_buffer_begin_info.flags =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_begin_info.pInheritanceInfo = NULL;

  vk_result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
  if (vk_result)
    return OWL_ERROR_FATAL;

  framebuffer = renderer->swapchain_framebuffers[renderer->swapchain_image];
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.pNext = NULL;
  render_pass_begin_info.renderPass = renderer->main_render_pass;
  render_pass_begin_info.framebuffer = framebuffer;
  render_pass_begin_info.renderArea.offset.x = 0;
  render_pass_begin_info.renderArea.offset.y = 0;
  render_pass_begin_info.renderArea.extent.width = renderer->width;
  render_pass_begin_info.renderArea.extent.height = renderer->height;
  render_pass_begin_info.clearValueCount = clear_value_count;
  render_pass_begin_info.pClearValues = clear_values;

  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  return OWL_OK;
}

OWL_PUBLIC owl_code
owl_renderer_end_frame(struct owl_renderer *renderer) {
  VkPipelineStageFlagBits stage;
  VkSubmitInfo submit_info;
  VkPresentInfoKHR present_info;

  uint32_t const frame = renderer->frame;
  VkCommandBuffer command_buffer = renderer->submit_command_buffers[frame];
  VkFence in_flight_fence = renderer->in_flight_fences[frame];
  VkSemaphore acquire_semaphore = renderer->acquire_semaphores[frame];
  VkSemaphore render_done_semaphore = renderer->render_done_semaphores[frame];

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

  vk_result = vkQueueSubmit(renderer->graphics_queue, 1, &submit_info,
                            in_flight_fence);
  if (vk_result)
    return OWL_ERROR_FATAL;

  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_done_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &renderer->swapchain;
  present_info.pImageIndices = &renderer->swapchain_image;
  present_info.pResults = NULL;

  vk_result = vkQueuePresentKHR(renderer->present_queue, &present_info);
  if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result))
    return owl_renderer_resize_swapchain(renderer);

  renderer->frame = (renderer->frame + 1) % renderer->frame_count;
  renderer->vertex_buffer_offset = 0;
  renderer->index_buffer_offset = 0;
  renderer->uniform_buffer_offset = 0;

  return OWL_OK;
}

OWL_PUBLIC void *
owl_renderer_upload_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_upload_allocation *allocation) {
  VkResult vk_result;

  if (renderer->upload_buffer_in_use)
    goto error;

  renderer->upload_buffer_size = size;

  {
    VkBufferCreateInfo buffer_create_info;

    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = NULL;
    buffer_create_info.flags = 0;
    buffer_create_info.size = size;
    buffer_create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.queueFamilyIndexCount = 0;
    buffer_create_info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(renderer->device, &buffer_create_info, NULL,
                               &renderer->upload_buffer);
    if (vk_result)
      goto error;
  }

  {
    VkMemoryPropertyFlagBits memory_properties;
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_allocate_info;

    memory_properties = 0;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    memory_properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(renderer->device, renderer->upload_buffer,
                                  &memory_requirements);

    memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_allocate_info.pNext = NULL;
    memory_allocate_info.allocationSize = memory_requirements.size;
    memory_allocate_info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, memory_requirements.memoryTypeBits, memory_properties);

    vk_result = vkAllocateMemory(renderer->device, &memory_allocate_info, NULL,
                                 &renderer->upload_buffer_memory);
    if (vk_result)
      goto error_destroy_buffer;

    vk_result = vkBindBufferMemory(renderer->device, renderer->upload_buffer,
                                   renderer->upload_buffer_memory, 0);
    if (vk_result)
      goto error_free_memory;

    vk_result = vkMapMemory(renderer->device, renderer->upload_buffer_memory,
                            0, renderer->upload_buffer_size, 0,
                            &renderer->upload_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  allocation->buffer = renderer->upload_buffer;

  return renderer->upload_buffer_data;

error_free_memory:
  vkFreeMemory(renderer->device, renderer->upload_buffer_memory, NULL);

error_destroy_buffer:
  vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);

error:
  return NULL;
}

OWL_PUBLIC void
owl_renderer_upload_free(struct owl_renderer *renderer, void *ptr) {
  OWL_UNUSED(ptr);
  OWL_ASSERT(renderer->upload_buffer_data == ptr);
  renderer->upload_buffer_in_use = 0;
  renderer->upload_buffer_size = 0;
  renderer->upload_buffer_data = NULL;
  
  vkFreeMemory(renderer->device, renderer->upload_buffer_memory, NULL);
  vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);
  
}

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_HEIGHT * OWL_FONT_ATLAS_WIDTH)

OWL_PUBLIC owl_code
owl_renderer_load_font(struct owl_renderer *renderer, uint32_t size,
                       char const *path) {
  owl_code code;
  uint8_t *bitmap;
  stbtt_pack_context pack;
  struct owl_plataform_file file;
  struct owl_texture_desc texture_desc;
  int32_t stb_result;
  int32_t const width = OWL_FONT_ATLAS_WIDTH;
  int32_t const height = OWL_FONT_ATLAS_HEIGHT;
  int32_t const first = OWL_RENDERER_FONT_FIRST_CHAR;
  int32_t const num = OWL_RENDERER_CHAR_COUNT;

  if (renderer->font_loaded)
    owl_renderer_unload_font(renderer);

  code = owl_plataform_load_file(path, &file);
  if (code)
    return code;

  bitmap = OWL_CALLOC(OWL_FONT_ATLAS_SIZE, sizeof(uint8_t));
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

  stb_result = stbtt_PackFontRange(
      &pack, file.data, 0, size, first, num,
      (stbtt_packedchar *)(&renderer->font_chars[0]));
  if (!stb_result) {
    code = OWL_ERROR_FATAL;
    goto error_pack_end;
  }

  texture_desc.source = OWL_TEXTURE_SOURCE_DATA;
  texture_desc.type = OWL_TEXTURE_TYPE_2D;
  texture_desc.path = NULL;
  texture_desc.pixels = bitmap;
  texture_desc.width = OWL_FONT_ATLAS_WIDTH;
  texture_desc.height = OWL_FONT_ATLAS_HEIGHT;
  texture_desc.format = OWL_PIXEL_FORMAT_R8_UNORM;

  code = owl_texture_init(renderer, &texture_desc, &renderer->font_atlas);
  if (!code)
    renderer->font_loaded = 1;

error_pack_end:
  stbtt_PackEnd(&pack);

error_free_bitmap:
  OWL_FREE(bitmap);

error_unload_file:
  owl_plataform_unload_file(&file);

  return code;
}

OWL_PUBLIC void
owl_renderer_unload_font(struct owl_renderer *renderer) {
  owl_texture_deinit(renderer, &renderer->font_atlas);
  renderer->font_loaded = 0;
}

#define OWL_MAX_SKYBOX_PATH_LENGTH 256

OWL_PUBLIC owl_code
owl_renderer_load_skybox(struct owl_renderer *renderer, char const *path) {
  owl_code code;
  struct owl_texture_desc texture_desc;

  texture_desc.type = OWL_TEXTURE_TYPE_CUBE;
  texture_desc.source = OWL_TEXTURE_SOURCE_FILE;
  texture_desc.path = path;
  texture_desc.pixels = NULL;
  texture_desc.width = 0;
  texture_desc.height = 0;
  texture_desc.format = OWL_PIXEL_FORMAT_R8G8B8A8_SRGB;

  code = owl_texture_init(renderer, &texture_desc, &renderer->skybox);
  if (!code)
    renderer->skybox_loaded = 1;

  return code;
}

OWL_PUBLIC void
owl_renderer_unload_skybox(struct owl_renderer *renderer) {
  owl_texture_deinit(renderer, &renderer->skybox);
  renderer->skybox_loaded = 0;
}

OWL_PUBLIC uint32_t
owl_renderer_find_memory_type(struct owl_renderer *renderer, uint32_t filter,
                              uint32_t properties) {
  uint32_t type;
  VkPhysicalDeviceMemoryProperties memory_properties;

  vkGetPhysicalDeviceMemoryProperties(renderer->physical_device,
                                      &memory_properties);

  for (type = 0; type < memory_properties.memoryTypeCount; ++type) {
    uint32_t flags = memory_properties.memoryTypes[type].propertyFlags;

    if ((flags & properties) && (filter & (1U << type)))
      return type;
  }

  return (uint32_t)-1;
}

OWL_PUBLIC owl_code
owl_renderer_begin_immediate_command_buffer(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  OWL_ASSERT(!renderer->immediate_command_buffer);

  {
    VkCommandBufferAllocateInfo command_buffer_allocate_info;
    VkCommandBuffer *command_buffer = &renderer->immediate_command_buffer;

    command_buffer_allocate_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = renderer->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(
        renderer->device, &command_buffer_allocate_info, command_buffer);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkCommandBufferBeginInfo command_buffer_begin_info;
    VkCommandBuffer command_buffer = renderer->immediate_command_buffer;

    command_buffer_begin_info.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = NULL;
    command_buffer_begin_info.flags =
        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    command_buffer_begin_info.pInheritanceInfo = NULL;

    vk_result = vkBeginCommandBuffer(command_buffer,
                                     &command_buffer_begin_info);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_immediate_command_buffer_deinit;
    }
  }

  goto out;

error_immediate_command_buffer_deinit:
  vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1,
                       &renderer->immediate_command_buffer);

out:
  return code;
}

OWL_PUBLIC owl_code
owl_renderer_end_immediate_command_buffer(struct owl_renderer *renderer) {
  VkSubmitInfo submit_info;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  OWL_ASSERT(renderer->immediate_command_buffer);

  vk_result = vkEndCommandBuffer(renderer->immediate_command_buffer);
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
  submit_info.pCommandBuffers = &renderer->immediate_command_buffer;
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

  renderer->immediate_command_buffer = VK_NULL_HANDLE;

cleanup:
  vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1,
                       &renderer->immediate_command_buffer);

  return code;
}

OWL_PUBLIC owl_code
owl_renderer_fill_glyph(struct owl_renderer *renderer, char c, owl_v2 offset,
                        struct owl_glyph *glyph) {
  stbtt_aligned_quad quad;
  owl_code code = OWL_OK;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&renderer->font_chars[0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_RENDERER_FONT_FIRST_CHAR, &offset[0], &offset[1],
                      &quad, 1);

  OWL_V3_SET(glyph->positions[0], quad.x0, quad.y0, 0.0F);
  OWL_V3_SET(glyph->positions[1], quad.x1, quad.y0, 0.0F);
  OWL_V3_SET(glyph->positions[2], quad.x0, quad.y1, 0.0F);
  OWL_V3_SET(glyph->positions[3], quad.x1, quad.y1, 0.0F);

  OWL_V2_SET(glyph->uvs[0], quad.s0, quad.t0);
  OWL_V2_SET(glyph->uvs[1], quad.s1, quad.t0);
  OWL_V2_SET(glyph->uvs[2], quad.s0, quad.t1);
  OWL_V2_SET(glyph->uvs[3], quad.s1, quad.t1);

  return code;
}

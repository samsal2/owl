#include "owl_renderer.h"

#include "owl_definitions.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_texture.h"
#include "owl_vector_math.h"
#include "stb_truetype.h"
#include "vulkan/vulkan_core.h"

#include <stdio.h>

#define OWL_DEFAULT_BUFFER_SIZE (1 << 16)

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32 owl_renderer_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
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

static char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif

static owl_code owl_renderer_init_instance(struct owl_renderer *renderer) {
  {
    owl_code code;
    VkResult vk_result = VK_SUCCESS;

    uint32_t extension_count;
    char const *const *extensions;

    VkApplicationInfo app_info;
    VkInstanceCreateInfo info;

    char const *name = owl_plataform_get_title(renderer->plataform);

    code = owl_plataform_get_required_instance_extensions(
        renderer->plataform, &extension_count, &extensions);
    if (code)
      return code;

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);

    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.pApplicationInfo = &app_info;
#if defined(OWL_ENABLE_VALIDATION)
    info.enabledLayerCount = OWL_ARRAY_SIZE(debug_validation_layers);
    info.ppEnabledLayerNames = debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
    instance_create_info.enabledLayerCount = 0;
    instance_create_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
    info.enabledExtensionCount = extension_count;
    info.ppEnabledExtensionNames = extensions;

    vk_result = vkCreateInstance(&info, NULL, &renderer->instance);
    if (vk_result)
      goto error;
  }

#if defined(OWL_ENABLE_VALIDATION)
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

  renderer->vk_debug_marker_set_object_name_ext =
      (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(
          renderer->instance, "vkDebugMarkerSetObjectNameEXT");
  if (!renderer->vk_destroy_debug_utils_messenger_ext)
    goto error_destroy_instance;

  {
    VkResult vk_result;
    VkDebugUtilsMessengerCreateInfoEXT info;

    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.pNext = NULL;
    info.flags = 0;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = owl_renderer_debug_callback;
    info.pUserData = renderer;

    vk_result = renderer->vk_create_debug_utils_messenger_ext(
        renderer->instance, &info, NULL, &renderer->debug_messenger);
    if (vk_result)
      goto error_destroy_instance;
  }
#else
  renderer->debug_messenger = NULL;
  renderer->vk_create_debug_utils_messenger_ext = NULL;
  renderer->vk_destroy_debug_utils_messenger_ext = NULL;
  renderer->vk_debug_marker_set_object_name_ext = NULL;
#endif

  return OWL_OK;

#if defined(OWL_ENABLE_VALIDATION)
error_destroy_instance:
  vkDestroyInstance(renderer->instance, NULL);
#endif

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_instance(struct owl_renderer *renderer) {
  VkInstance const instance = renderer->instance;
#if defined(OWL_ENABLE_VALIDATION)
  renderer->vk_destroy_debug_utils_messenger_ext(
      instance, renderer->debug_messenger, NULL);
#endif
  vkDestroyInstance(instance, NULL);
}

#if defined(OWL_ENABLE_VALIDATION) && 0
#define OWL_RENDERER_SET_VK_OBJECT_NAME(renderer, handle, type, name)         \
  owl_renderer_set_vk_object_name(renderer, handle, type, name)
static void owl_renderer_set_vk_object_name(struct owl_renderer *renderer,
                                            uint64_t handle,
                                            VkDebugReportObjectTypeEXT type,
                                            char const *name) {
  VkDebugMarkerObjectNameInfoEXT info;
  VkDevice const device = renderer->device;

  info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
  info.pNext = NULL;
  info.objectType = type;
  info.object = handle;
  info.pObjectName = name;

  renderer->vk_debug_marker_set_object_name_ext(device, &info);
}
#else
#define OWL_RENDERER_SET_VK_OBJECT_NAME(renderer, handle, type, name)
#endif

static owl_code owl_renderer_init_surface(struct owl_renderer *renderer) {
  return owl_plataform_create_vulkan_surface(renderer->plataform, renderer);
}

static void owl_renderer_deinit_surface(struct owl_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

static char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

/* TODO(samuel): refactor this function, it's way too big */
static owl_code
owl_renderer_select_physical_device(struct owl_renderer *renderer) {
  uint32_t i;

  int32_t device_found = 0;

  owl_code code = OWL_OK;
  VkResult vk_result = VK_SUCCESS;

  uint32_t device_count = 0;
  VkPhysicalDevice *devices = NULL;

  uint32_t queue_family_properties_count = 0;
  VkQueueFamilyProperties *queue_family_properties = NULL;

  int32_t *extensions_found = NULL;
  uint32_t extension_property_count = 0;
  VkExtensionProperties *extension_properties = NULL;

  uint32_t format_count = 0;
  VkSurfaceFormatKHR *formats = NULL;

  extensions_found = OWL_MALLOC(OWL_ARRAY_SIZE(device_extensions) *
                                sizeof(*extensions_found));
  if (!extensions_found) {
    code = OWL_ERROR_NO_MEMORY;
    goto cleanup;
  }

  vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count,
                                         NULL);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  devices = OWL_MALLOC(device_count * sizeof(*devices));
  if (!devices) {
    code = OWL_ERROR_NO_MEMORY;
    goto cleanup;
  }

  vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count,
                                         devices);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  for (i = 0; i < device_count && !device_found; ++i) {
    uint32_t j;
    VkPhysicalDeviceProperties device_properties;
    VkSampleCountFlagBits requested_samples = VK_SAMPLE_COUNT_2_BIT;
    VkSampleCountFlagBits supported_samples = 0;
    VkFormat requested_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkColorSpaceKHR requested_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    uint32_t present_mode_count;
    VkPresentModeKHR present_modes[16];
    int32_t found_present_mode = 0;
    VkPresentModeKHR requested_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    VkFormatProperties d24_unorm_s8_uint_properties;
    VkFormatProperties d32_sfloat_s8_uint_properties;
    int32_t found_format = 0;
    uint32_t current_family = 0;
    void *check_realloc = NULL;
    uint32_t graphics_family = (uint32_t)-1;
    uint32_t present_family = (uint32_t)-1;
    uint32_t compute_family = (uint32_t)-1;
    uint32_t has_formats = 0;
    uint32_t has_present_modes = 0;
    int32_t has_extensions = 0;
    VkPhysicalDevice const device = devices[i];

    OWL_DEBUG_LOG("looking at device %p!\n", device);

    /* has formats? if not go next device */
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, renderer->surface,
                                         &has_formats, NULL);
    if (!has_formats)
      continue;

    OWL_DEBUG_LOG("  device %p contains surface formats!\n", device);

    /* has present modes? if not go next device */
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, renderer->surface,
                                              &has_present_modes, NULL);
    if (!has_present_modes)
      continue;

    OWL_DEBUG_LOG("  device %p contains present modes!\n", device);

    /* allocate the queue_family_properties, no memory? cleanup and return */
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queue_family_properties_count, NULL);

    check_realloc = OWL_REALLOC(queue_family_properties,
                                queue_family_properties_count *
                                    sizeof(*queue_family_properties));
    if (!check_realloc) {
      code = OWL_ERROR_NO_MEMORY;
      goto cleanup;
    }

    queue_family_properties = check_realloc;
    vkGetPhysicalDeviceQueueFamilyProperties(
        device, &queue_family_properties_count, queue_family_properties);

    /* for each family, if said family has the required properties set the
       corresponding family to it's index */
    for (current_family = 0;
         current_family < queue_family_properties_count &&
         ((uint32_t)-1 == graphics_family || (uint32_t)-1 == present_family ||
          (uint32_t)-1 == compute_family);
         ++current_family) {
      VkBool32 has_surface;
      VkQueueFamilyProperties *properties;

      properties = &queue_family_properties[current_family];

      vkGetPhysicalDeviceSurfaceSupportKHR(device, current_family,
                                           renderer->surface, &has_surface);

      OWL_DEBUG_LOG("  current_family %u\n", current_family);

      if (properties->queueFlags & VK_QUEUE_GRAPHICS_BIT)
        graphics_family = current_family;

      if (properties->queueFlags & VK_QUEUE_COMPUTE_BIT)
        compute_family = current_family;

      if (has_surface)
        present_family = current_family;
    }

    OWL_DEBUG_LOG("  device %p had the following families %u, %u, %u!\n",
                  device, graphics_family, present_family, compute_family);

    /* if one family is missing, go next device */
    if ((uint32_t)-1 == graphics_family || (uint32_t)-1 == present_family ||
        (uint32_t)-1 == compute_family)
      continue;

    OWL_DEBUG_LOG("  device %p has the required families!\n", device);

    /* vulkan error, cleanup and return */
    vk_result = vkEnumerateDeviceExtensionProperties(
        device, NULL, &extension_property_count, NULL);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto cleanup;
    }

    check_realloc = OWL_REALLOC(extension_properties,
                                extension_property_count *
                                    sizeof(*extension_properties));
    if (!check_realloc) {
      code = OWL_ERROR_NO_MEMORY;
      goto cleanup;
    }

    extension_properties = check_realloc;
    vk_result = vkEnumerateDeviceExtensionProperties(
        device, NULL, &extension_property_count, extension_properties);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto cleanup;
    }

    for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j)
      extensions_found[j] = 0;

    for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j) {
      uint32_t k;
      char const *required = device_extensions[j];
      uint32_t const required_length = OWL_STRLEN(required);

      OWL_DEBUG_LOG("  device %p looking at %s\n", device, required);

      for (k = 0; k < extension_property_count && !extensions_found[j]; ++k) {
        uint32_t min_length;
        char const *current;
        uint32_t const max_length = VK_MAX_EXTENSION_NAME_SIZE;

        current = extension_properties[k].extensionName;
        min_length = OWL_MIN(required_length, max_length);

        OWL_DEBUG_LOG("    comparing to %s\n", current);

        if (!OWL_STRNCMP(required, current, min_length))
          extensions_found[j] = 1;
      }
    }

    has_extensions = 1;
    for (j = 0; j < OWL_ARRAY_SIZE(device_extensions) && has_extensions; ++j)
      if (!extensions_found[j])
        has_extensions = 0;

    /* if it's missing device extensions, go next */
    if (!has_extensions)
      continue;

    OWL_DEBUG_LOG("  device %p has the required extensions!\n", device);

    /* congratulations! a device has been found! */
    device_found = 1;
    renderer->physical_device = device;
    renderer->graphics_family = graphics_family;
    renderer->present_family = present_family;
    renderer->compute_family = compute_family;

    vkGetPhysicalDeviceProperties(device, &device_properties);

    supported_samples = 0;
    supported_samples |= device_properties.limits.framebufferColorSampleCounts;
    supported_samples |= device_properties.limits.framebufferDepthSampleCounts;

    if (VK_SAMPLE_COUNT_1_BIT & requested_samples) {
      OWL_DEBUG_LOG("VK_SAMPLE_COUNT_1_BIT not supporeted, falling back to "
                    "VK_SAMPLE_COUNT_2_BIT\n");
      renderer->msaa = VK_SAMPLE_COUNT_2_BIT;
    } else if (supported_samples & requested_samples) {
      renderer->msaa = requested_samples;
    } else {
      OWL_DEBUG_LOG("falling back to VK_SAMPLE_COUNT_2_BIT\n");
      renderer->msaa = VK_SAMPLE_COUNT_2_BIT;
    }

    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, renderer->surface,
                                                     &format_count, NULL);

    check_realloc = OWL_REALLOC(formats, format_count * sizeof(*formats));
    if (!check_realloc) {
      code = OWL_ERROR_NO_MEMORY;
      goto cleanup;
    }

    formats = check_realloc;
    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, renderer->surface,
                                                     &format_count, formats);

    for (j = 0; j < format_count && !found_format; ++j)
      if (formats[j].format == requested_format &&
          formats[j].colorSpace == requested_color_space)
        found_format = 1;

    /* no suitable format, go next */
    if (!found_format) {
      code = OWL_ERROR_FATAL;
      goto cleanup;
    }

    renderer->surface_format.format = requested_format;
    renderer->surface_format.colorSpace = requested_color_space;

    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, renderer->surface, &present_mode_count, NULL);

    if (vk_result || OWL_ARRAY_SIZE(present_modes) < present_mode_count) {
      code = OWL_ERROR_FATAL;
      goto cleanup;
    }

    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, renderer->surface, &present_mode_count, present_modes);

    for (j = 0; j < present_mode_count; ++j)
      if (requested_present_mode == present_modes[j])
        found_present_mode = 1;

    /* no suitable present mode, go next */
    if (!found_present_mode) {
      OWL_DEBUG_LOG("falling back to VK_PRESENT_MODE_FIFO_KHR\n");
      renderer->present_mode = VK_PRESENT_MODE_FIFO_KHR;
    } else {
      renderer->present_mode = requested_present_mode;
    }

    vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_D24_UNORM_S8_UINT,
                                        &d24_unorm_s8_uint_properties);

    vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                        &d32_sfloat_s8_uint_properties);

    if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
        d24_unorm_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    } else if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
               d32_sfloat_s8_uint_properties.optimalTilingFeatures) {
      renderer->depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    } else {
      OWL_DEBUG_LOG("could't find depth format\n");
      code = OWL_ERROR_FATAL;
      goto cleanup;
    }
  }

  if (!device_found)
    code = OWL_ERROR_FATAL;

cleanup:
  OWL_FREE(devices);
  OWL_FREE(queue_family_properties);
  OWL_FREE(extensions_found);
  OWL_FREE(extension_properties);
  OWL_FREE(formats);

  return code;
}

static owl_code owl_renderer_init_device(struct owl_renderer *renderer) {
  VkPhysicalDeviceFeatures features;
  VkDeviceCreateInfo info;
  VkDeviceQueueCreateInfo queue_infos[2];
  float const priority = 1.0F;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = NULL;
  queue_infos[0].flags = 0;
  queue_infos[0].queueFamilyIndex = renderer->graphics_family;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &priority;

  queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[1].pNext = NULL;
  queue_infos[1].flags = 0;
  queue_infos[1].queueFamilyIndex = renderer->present_family;
  queue_infos[1].queueCount = 1;
  queue_infos[1].pQueuePriorities = &priority;

  vkGetPhysicalDeviceFeatures(renderer->physical_device, &features);

  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  if (renderer->graphics_family == renderer->present_family) {
    info.queueCreateInfoCount = 1;
  } else {
    info.queueCreateInfoCount = 2;
  }
  info.pQueueCreateInfos = queue_infos;
  info.enabledLayerCount = 0;      /* deprecated */
  info.ppEnabledLayerNames = NULL; /* deprecated */
  info.enabledExtensionCount = OWL_ARRAY_SIZE(device_extensions);
  info.ppEnabledExtensionNames = device_extensions;
  info.pEnabledFeatures = &features;

  vk_result = vkCreateDevice(renderer->physical_device, &info, NULL,
                             &renderer->device);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vkGetDeviceQueue(renderer->device, renderer->graphics_family, 0,
                   &renderer->graphics_queue);

  vkGetDeviceQueue(renderer->device, renderer->present_family, 0,
                   &renderer->present_queue);

  vkGetDeviceQueue(renderer->device, renderer->present_family, 0,
                   &renderer->compute_queue);

  return code;
}

static void owl_renderer_deinit_device(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

static owl_code owl_renderer_clamp_dimensions(struct owl_renderer *renderer) {
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

static owl_code owl_renderer_init_attachments(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;

  {
    VkImageCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = renderer->surface_format.format;
    info.extent.width = renderer->width;
    info.extent.height = renderer->height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = renderer->msaa;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(device, &info, NULL, &renderer->color_image);
    if (vk_result)
      goto error;
  }

  {
    VkMemoryPropertyFlagBits properties;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetImageMemoryRequirements(device, renderer->color_image, &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL, &renderer->color_memory);
    if (vk_result)
      goto error_destroy_color_image;

    vk_result = vkBindImageMemory(device, renderer->color_image,
                                  renderer->color_memory, 0);
    if (vk_result)
      goto error_free_color_memory;
  }

  {
    VkImageViewCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = renderer->color_image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = renderer->surface_format.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(device, &info, NULL,
                                  &renderer->color_image_view);
    if (vk_result)
      goto error_free_color_memory;
  }

  {
    VkImageCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = renderer->depth_format;
    info.extent.width = renderer->width;
    info.extent.height = renderer->height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = renderer->msaa;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage(renderer->device, &info, NULL,
                              &renderer->depth_image);
    if (vk_result)
      goto error_destroy_color_image_view;
  }

  {
    VkMemoryPropertyFlagBits properties;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vkGetImageMemoryRequirements(device, renderer->depth_image, &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL, &renderer->depth_memory);
    if (vk_result)
      goto error_destroy_depth_image;

    vk_result = vkBindImageMemory(device, renderer->depth_image,
                                  renderer->depth_memory, 0);
    if (vk_result)
      goto error_free_depth_memory;
  }
  {
    VkImageViewCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = renderer->depth_image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = renderer->depth_format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(device, &info, NULL,
                                  &renderer->depth_image_view);
    if (vk_result)
      goto error_free_depth_memory;
  }

  return OWL_OK;

error_free_depth_memory:
  vkFreeMemory(device, renderer->depth_memory, NULL);

error_destroy_depth_image:
  vkDestroyImage(device, renderer->depth_image, NULL);

error_destroy_color_image_view:
  vkDestroyImageView(device, renderer->color_image_view, NULL);

error_free_color_memory:
  vkFreeMemory(device, renderer->color_memory, NULL);

error_destroy_color_image:
  vkDestroyImage(device, renderer->color_image, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_attachments(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyImageView(device, renderer->depth_image_view, NULL);
  vkFreeMemory(device, renderer->depth_memory, NULL);
  vkDestroyImage(device, renderer->depth_image, NULL);
  vkDestroyImageView(device, renderer->color_image_view, NULL);
  vkFreeMemory(device, renderer->color_memory, NULL);
  vkDestroyImage(device, renderer->color_image, NULL);
}

static owl_code
owl_renderer_init_render_passes(struct owl_renderer *renderer) {
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo info;
  VkResult vk_result;
  VkDevice const device = renderer->device;

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

  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.attachmentCount = OWL_ARRAY_SIZE(attachments);
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  vk_result = vkCreateRenderPass(device, &info, NULL,
                                 &renderer->main_render_pass);
  if (vk_result)
    goto error;

  return OWL_OK;

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_render_passes(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyRenderPass(device, renderer->main_render_pass, NULL);
}

static owl_code owl_renderer_init_swapchain(struct owl_renderer *renderer) {
  int32_t i;

  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;
  VkDevice const device = renderer->device;
  VkPhysicalDevice const physical_device = renderer->physical_device;

  {
    uint32_t families[2];
    VkSwapchainCreateInfoKHR info;
    VkSurfaceCapabilitiesKHR capabilities;

    renderer->swapchain_image = 0;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physical_device, renderer->surface, &capabilities);

    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.pNext = NULL;
    info.flags = 0;
    info.surface = renderer->surface;
    info.minImageCount = renderer->frame_count;
    info.imageFormat = renderer->surface_format.format;
    info.imageColorSpace = renderer->surface_format.colorSpace;
    info.imageExtent.width = renderer->width;
    info.imageExtent.height = renderer->height;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.preTransform = capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = renderer->present_mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = VK_NULL_HANDLE;

    families[0] = renderer->graphics_family;
    families[1] = renderer->present_family;

    if (renderer->graphics_family == renderer->present_family) {
      info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      info.queueFamilyIndexCount = 0;
      info.pQueueFamilyIndices = NULL;
    } else {
      info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      info.queueFamilyIndexCount = 2;
      info.pQueueFamilyIndices = families;
    }

    vk_result = vkCreateSwapchainKHR(device, &info, NULL,
                                     &renderer->swapchain);
    if (vk_result)
      goto error;

    vk_result = vkGetSwapchainImagesKHR(
        device, renderer->swapchain, &renderer->swapchain_image_count, NULL);
    if (vk_result ||
        OWL_SWAPCHAIN_IMAGE_MAX <= renderer->swapchain_image_count)
      goto error_destroy_swapchain;

    vk_result = vkGetSwapchainImagesKHR(device, renderer->swapchain,
                                        &renderer->swapchain_image_count,
                                        renderer->swapchain_images);
    if (vk_result)
      goto error_destroy_swapchain;
  }

  for (i = 0; i < (int32_t)renderer->swapchain_image_count; ++i) {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = renderer->swapchain_images[i];
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = renderer->surface_format.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(device, &info, NULL,
                                  &renderer->swapchain_image_views[i]);
    if (vk_result)
      goto error_destroy_image_views;
  }

  for (i = 0; i < (int32_t)renderer->swapchain_image_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo info;

    attachments[0] = renderer->color_image_view;
    attachments[1] = renderer->depth_image_view;
    attachments[2] = renderer->swapchain_image_views[i];

    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.renderPass = renderer->main_render_pass;
    info.attachmentCount = OWL_ARRAY_SIZE(attachments);
    info.pAttachments = attachments;
    info.width = renderer->width;
    info.height = renderer->height;
    info.layers = 1;

    vk_result = vkCreateFramebuffer(device, &info, NULL,
                                    &renderer->swapchain_framebuffers[i]);
    if (vk_result)
      goto error_destroy_framebuffers;
  }

  return OWL_OK;

error_destroy_framebuffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFramebuffer(device, renderer->swapchain_framebuffers[i], NULL);

  i = renderer->swapchain_image_count;

error_destroy_image_views:
  for (i = i - 1; i >= 0; --i)
    vkDestroyImageView(device, renderer->swapchain_image_views[i], NULL);

error_destroy_swapchain:
  vkDestroySwapchainKHR(device, renderer->swapchain, NULL);

error:
  return code;
}

static void owl_renderer_deinit_swapchain(struct owl_renderer *renderer) {
  uint32_t i;
  VkDevice const device = renderer->device;

  for (i = 0; i < renderer->swapchain_image_count; ++i)
    vkDestroyFramebuffer(device, renderer->swapchain_framebuffers[i], NULL);

  for (i = 0; i < renderer->swapchain_image_count; ++i)
    vkDestroyImageView(device, renderer->swapchain_image_views[i], NULL);

  vkDestroySwapchainKHR(device, renderer->swapchain, NULL);
}

static owl_code owl_renderer_init_pools(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;

  {
    VkCommandPoolCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = renderer->graphics_family;

    vk_result = vkCreateCommandPool(device, &info, NULL,
                                    &renderer->command_pool);
    if (vk_result)
      goto error;
  }

  {
    VkDescriptorPoolSize sizes[7];
    VkDescriptorPoolCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

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

    sizes[6].descriptorCount = 256;
    sizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = 256 * OWL_ARRAY_SIZE(sizes);
    info.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    info.pPoolSizes = sizes;

    vk_result = vkCreateDescriptorPool(device, &info, NULL,
                                       &renderer->descriptor_pool);
    if (vk_result)
      goto error_destroy_command_pool;
  }

  return OWL_OK;

error_destroy_command_pool:
  vkDestroyCommandPool(device, renderer->command_pool, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_pools(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyDescriptorPool(device, renderer->descriptor_pool, NULL);
  vkDestroyCommandPool(device, renderer->command_pool, NULL);
}

static owl_code owl_renderer_init_shaders(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    /* TODO(samuel): Properly load code at runtime */
    static uint32_t const spv[] = {
#include "owl_basic.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->basic_vertex_shader);
    if (vk_result)
      goto error;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_basic.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->basic_fragment_shader);
    if (vk_result)
      goto error_destroy_basic_vertex_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_font.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->text_fragment_shader);

    if (vk_result)
      goto error_destroy_basic_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_model.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->model_vertex_shader);

    if (VK_SUCCESS != vk_result)
      goto error_destroy_text_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_model.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->model_fragment_shader);

    if (vk_result)
      goto error_destroy_model_vertex_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_skybox.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->skybox_vertex_shader);

    if (vk_result)
      goto error_destroy_model_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_skybox.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->skybox_fragment_shader);

    if (vk_result)
      goto error_destroy_skybox_vertex_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_advect.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(
        device, &info, NULL, &renderer->fluid_simulation_advect_shader);

    if (vk_result)
      goto error_destroy_skybox_fragment_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_curl.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->fluid_simulation_curl_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_advect_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_diverge.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(
        device, &info, NULL, &renderer->fluid_simulation_diverge_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_curl_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_gradient_subtract.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(
        device, &info, NULL,
        &renderer->fluid_simulation_gradient_subtract_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_diverge_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_pressure.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(
        device, &info, NULL, &renderer->fluid_simulation_pressure_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_gradient_subtract_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_splat.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(device, &info, NULL,
                                     &renderer->fluid_simulation_splat_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_pressure_shader;
  }

  {
    VkShaderModuleCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    static uint32_t const spv[] = {
#include "owl_fluid_vorticity.comp.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(
        device, &info, NULL, &renderer->fluid_simulation_vorticity_shader);

    if (vk_result)
      goto error_destroy_fluid_simulation_splat_shader;
  }

  return OWL_OK;

error_destroy_fluid_simulation_splat_shader:
  vkDestroyShaderModule(device, renderer->fluid_simulation_splat_shader, NULL);

error_destroy_fluid_simulation_pressure_shader:
  vkDestroyShaderModule(device, renderer->fluid_simulation_pressure_shader,
                        NULL);

error_destroy_fluid_simulation_gradient_subtract_shader:
  vkDestroyShaderModule(
      device, renderer->fluid_simulation_gradient_subtract_shader, NULL);

error_destroy_fluid_simulation_diverge_shader:
  vkDestroyShaderModule(device, renderer->fluid_simulation_diverge_shader,
                        NULL);

error_destroy_fluid_simulation_curl_shader:
  vkDestroyShaderModule(device, renderer->fluid_simulation_curl_shader, NULL);

error_destroy_fluid_simulation_advect_shader:
  vkDestroyShaderModule(device, renderer->fluid_simulation_advect_shader,
                        NULL);

error_destroy_skybox_fragment_shader:
  vkDestroyShaderModule(device, renderer->skybox_fragment_shader, NULL);

error_destroy_skybox_vertex_shader:
  vkDestroyShaderModule(device, renderer->skybox_vertex_shader, NULL);

error_destroy_model_fragment_shader:
  vkDestroyShaderModule(device, renderer->model_fragment_shader, NULL);

error_destroy_model_vertex_shader:
  vkDestroyShaderModule(device, renderer->model_vertex_shader, NULL);

error_destroy_text_fragment_shader:
  vkDestroyShaderModule(device, renderer->text_fragment_shader, NULL);

error_destroy_basic_fragment_shader:
  vkDestroyShaderModule(device, renderer->basic_fragment_shader, NULL);

error_destroy_basic_vertex_shader:
  vkDestroyShaderModule(device, renderer->basic_vertex_shader, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_shaders(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyShaderModule(device, renderer->fluid_simulation_vorticity_shader,
                        NULL);
  vkDestroyShaderModule(device, renderer->fluid_simulation_splat_shader, NULL);
  vkDestroyShaderModule(device, renderer->fluid_simulation_pressure_shader,
                        NULL);
  vkDestroyShaderModule(
      device, renderer->fluid_simulation_gradient_subtract_shader, NULL);
  vkDestroyShaderModule(device, renderer->fluid_simulation_diverge_shader,
                        NULL);
  vkDestroyShaderModule(device, renderer->fluid_simulation_curl_shader, NULL);
  vkDestroyShaderModule(device, renderer->fluid_simulation_advect_shader,
                        NULL);
  vkDestroyShaderModule(device, renderer->skybox_fragment_shader, NULL);
  vkDestroyShaderModule(device, renderer->skybox_vertex_shader, NULL);
  vkDestroyShaderModule(device, renderer->model_fragment_shader, NULL);
  vkDestroyShaderModule(device, renderer->model_vertex_shader, NULL);
  vkDestroyShaderModule(device, renderer->text_fragment_shader, NULL);
  vkDestroyShaderModule(device, renderer->basic_fragment_shader, NULL);
  vkDestroyShaderModule(device, renderer->basic_vertex_shader, NULL);
}

static owl_code owl_renderer_init_layouts(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

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

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL, &renderer->common_uniform_descriptor_set_layout);
    if (vk_result)
      goto error;
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

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

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL, &renderer->common_texture_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_common_uniform_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayout layouts[2];
    VkPipelineLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    layouts[0] = renderer->common_uniform_descriptor_set_layout;
    layouts[1] = renderer->common_texture_descriptor_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = NULL;

    vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                       &renderer->common_pipeline_layout);
    if (vk_result)
      goto error_destroy_common_texture_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                         VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL, &renderer->model_uniform_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_common_pipeline_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

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

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL, &renderer->model_joints_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_model_uniform_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding bindings[4];
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

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

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = NULL;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = OWL_ARRAY_SIZE(bindings);
    info.pBindings = bindings;

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL, &renderer->model_material_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_model_joints_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayout layouts[3];
    VkPushConstantRange push_constant;
    VkPipelineLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(struct owl_model_material_push_constant);

    layouts[0] = renderer->model_uniform_descriptor_set_layout;
    layouts[1] = renderer->model_material_descriptor_set_layout;
    layouts[2] = renderer->model_joints_descriptor_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push_constant;

    vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                       &renderer->model_pipeline_layout);
    if (vk_result)
      goto error_destroy_model_material_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL,
        &renderer->fluid_simulation_uniform_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_model_pipeline_layout;
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    binding.pImmutableSamplers = NULL;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.bindingCount = 1;
    info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        device, &info, NULL,
        &renderer->fluid_simulation_source_descriptor_set_layout);
    if (vk_result)
      goto error_destroy_fluid_simulation_uniform_descriptor_set_layout;
  }

  {
    VkDescriptorSetLayout layouts[4];
    VkPushConstantRange push_constant;
    VkPipelineLayoutCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    push_constant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof(struct owl_model_material_push_constant);

    layouts[0] = renderer->fluid_simulation_uniform_descriptor_set_layout;
    layouts[1] = renderer->fluid_simulation_source_descriptor_set_layout;
    layouts[2] = renderer->fluid_simulation_source_descriptor_set_layout;
    layouts[3] = renderer->fluid_simulation_source_descriptor_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push_constant;

    vk_result = vkCreatePipelineLayout(
        device, &info, NULL, &renderer->fluid_simulation_pipeline_layout);
#if 0
    OWL_ASSERT(0 && "correct size");
#endif
    if (vk_result)
      goto error_destroy_fluid_simulation_source_descriptor_set_layout;
  }

  return OWL_OK;

error_destroy_fluid_simulation_source_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->fluid_simulation_source_descriptor_set_layout, NULL);

error_destroy_fluid_simulation_uniform_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->fluid_simulation_uniform_descriptor_set_layout, NULL);

error_destroy_model_pipeline_layout:
  vkDestroyPipelineLayout(device, renderer->model_pipeline_layout, NULL);

error_destroy_model_material_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->model_material_descriptor_set_layout, NULL);

error_destroy_model_joints_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->model_joints_descriptor_set_layout, NULL);

error_destroy_model_uniform_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->model_uniform_descriptor_set_layout, NULL);

error_destroy_common_pipeline_layout:
  vkDestroyPipelineLayout(device, renderer->common_pipeline_layout, NULL);

error_destroy_common_texture_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->common_texture_descriptor_set_layout, NULL);

error_destroy_common_uniform_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(
      device, renderer->common_uniform_descriptor_set_layout, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_layouts(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyPipelineLayout(device, renderer->fluid_simulation_pipeline_layout,
                          NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->fluid_simulation_source_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->fluid_simulation_uniform_descriptor_set_layout, NULL);
  vkDestroyPipelineLayout(device, renderer->model_pipeline_layout, NULL);
  vkDestroyPipelineLayout(device, renderer->common_pipeline_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->model_material_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->model_joints_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->model_uniform_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->common_texture_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(
      device, renderer->common_uniform_descriptor_set_layout, NULL);
}

static owl_code
owl_renderer_init_graphics_pipelines(struct owl_renderer *renderer) {
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
  VkGraphicsPipelineCreateInfo info;
  VkResult vk_result;
  VkDevice const device = renderer->device;

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

  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.stageCount = OWL_ARRAY_SIZE(stages);
  info.pStages = stages;
  info.pVertexInputState = &vertex_input;
  info.pInputAssemblyState = &input_assembly;
  info.pTessellationState = NULL;
  info.pViewportState = &viewport_state;
  info.pRasterizationState = &rasterization;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depth;
  info.pColorBlendState = &color;
  info.pDynamicState = NULL;
  info.layout = renderer->common_pipeline_layout;
  info.renderPass = renderer->main_render_pass;
  info.subpass = 0;
  info.basePipelineHandle = VK_NULL_HANDLE;
  info.basePipelineIndex = -1;

  vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
                                        &renderer->basic_pipeline);
  if (vk_result)
    goto error;

  rasterization.polygonMode = VK_POLYGON_MODE_LINE;

  vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
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

  vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
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

  info.layout = renderer->model_pipeline_layout;

  vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
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

  info.layout = renderer->common_pipeline_layout;

  vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, NULL,
                                        &renderer->skybox_pipeline);

  if (vk_result)
    goto error_destroy_model_pipeline;

  return OWL_OK;

error_destroy_model_pipeline:
  vkDestroyPipeline(device, renderer->model_pipeline, NULL);

error_destroy_text_pipeline:
  vkDestroyPipeline(device, renderer->text_pipeline, NULL);

error_destroy_wires_pipeline:
  vkDestroyPipeline(device, renderer->wires_pipeline, NULL);

error_destroy_basic_pipeline:
  vkDestroyPipeline(device, renderer->basic_pipeline, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void
owl_renderer_deinit_graphics_pipelines(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyPipeline(device, renderer->skybox_pipeline, NULL);
  vkDestroyPipeline(device, renderer->model_pipeline, NULL);
  vkDestroyPipeline(device, renderer->text_pipeline, NULL);
  vkDestroyPipeline(device, renderer->wires_pipeline, NULL);
  vkDestroyPipeline(device, renderer->basic_pipeline, NULL);
}

static owl_code
owl_renderer_init_compute_pipelines(struct owl_renderer *renderer) {
  VkComputePipelineCreateInfo info;
  VkResult vk_result = VK_SUCCESS;
  VkDevice const device = renderer->device;

  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.pNext = NULL;
  info.stage.flags = 0;
  info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  info.stage.module = renderer->fluid_simulation_advect_shader;
  info.stage.pName = "main";
  info.stage.pSpecializationInfo = NULL;
  info.layout = renderer->fluid_simulation_pipeline_layout;
  info.basePipelineHandle = VK_NULL_HANDLE;
  info.basePipelineIndex = -1;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_advect_pipeline);
  if (vk_result)
    goto error;

  info.stage.module = renderer->fluid_simulation_curl_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_curl_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_advect_pipeline;

  info.stage.module = renderer->fluid_simulation_diverge_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_diverge_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_curl_pipeline;

  info.stage.module = renderer->fluid_simulation_gradient_subtract_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_gradient_subtract_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_diverge_pipeline;

  info.stage.module = renderer->fluid_simulation_pressure_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_pressure_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_gradient_subtract_pipeline;

  info.stage.module = renderer->fluid_simulation_splat_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_splat_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_pressure_pipeline;

  info.stage.module = renderer->fluid_simulation_vorticity_shader;
  vk_result = vkCreateComputePipelines(
      device, VK_NULL_HANDLE, 1, &info, NULL,
      &renderer->fluid_simulation_vorticity_pipeline);
  if (vk_result)
    goto error_destroy_fluid_simulation_splat_pipeline;

  return OWL_OK;

error_destroy_fluid_simulation_splat_pipeline:
  vkDestroyPipeline(device, renderer->fluid_simulation_splat_pipeline, NULL);

error_destroy_fluid_simulation_pressure_pipeline:
  vkDestroyPipeline(device, renderer->fluid_simulation_pressure_pipeline,
                    NULL);

error_destroy_fluid_simulation_gradient_subtract_pipeline:
  vkDestroyPipeline(
      device, renderer->fluid_simulation_gradient_subtract_pipeline, NULL);

error_destroy_fluid_simulation_diverge_pipeline:
  vkDestroyPipeline(device, renderer->fluid_simulation_diverge_pipeline, NULL);

error_destroy_fluid_simulation_curl_pipeline:
  vkDestroyPipeline(device, renderer->fluid_simulation_curl_pipeline, NULL);

error_destroy_fluid_simulation_advect_pipeline:
  vkDestroyPipeline(device, renderer->fluid_simulation_advect_pipeline, NULL);

error:
  return OWL_ERROR_FATAL;
}

static void
owl_renderer_deinit_compute_pipelines(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroyPipeline(device, renderer->fluid_simulation_vorticity_pipeline,
                    NULL);
  vkDestroyPipeline(device, renderer->fluid_simulation_splat_pipeline, NULL);
  vkDestroyPipeline(device, renderer->fluid_simulation_pressure_pipeline,
                    NULL);
  vkDestroyPipeline(
      device, renderer->fluid_simulation_gradient_subtract_pipeline, NULL);
  vkDestroyPipeline(device, renderer->fluid_simulation_diverge_pipeline, NULL);
  vkDestroyPipeline(device, renderer->fluid_simulation_curl_pipeline, NULL);
  vkDestroyPipeline(device, renderer->fluid_simulation_advect_pipeline, NULL);
}

static owl_code
owl_renderer_init_upload_buffer(struct owl_renderer *renderer) {
  renderer->upload_buffer_in_use = 0;
  return OWL_OK;
}

static void owl_renderer_deinit_upload_buffer(struct owl_renderer *renderer) {
  if (renderer->upload_buffer_in_use) { /* someone forgot to call free */
    VkDevice const device = renderer->device;
    vkFreeMemory(device, renderer->upload_buffer_memory, NULL);
    vkDestroyBuffer(device, renderer->upload_buffer, NULL);
  }
}

static owl_code owl_renderer_init_garbage(struct owl_renderer *renderer) {
  uint32_t i;

  renderer->garbage = 0;

  for (i = 0; i < OWL_GARBAGE_FRAME_COUNT; ++i) {
    renderer->garbage_buffer_counts[i] = 0;
    renderer->garbage_memory_counts[i] = 0;
    renderer->garbage_descriptor_set_counts[i] = 0;
  }

  return OWL_OK;
}

static void owl_renderer_collect_garbage(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const garbage_frames = OWL_GARBAGE_FRAME_COUNT;
  /* use the _oldest_ garbage framt to collect */
  uint32_t const collect = (renderer->garbage + 2) % garbage_frames;
  VkDevice const device = renderer->device;

  /* update the garbage index */
  renderer->garbage = (renderer->garbage + 1) % garbage_frames;

  if (renderer->garbage_descriptor_set_counts[collect]) {
    vkFreeDescriptorSets(device, renderer->descriptor_pool,
                         renderer->garbage_descriptor_set_counts[collect],
                         renderer->garbage_descriptor_sets[collect]);
  }

  renderer->garbage_descriptor_set_counts[collect] = 0;

  for (i = 0; i < renderer->garbage_memory_counts[collect]; ++i) {
    VkDeviceMemory memory = renderer->garbage_memories[collect][i];
    vkFreeMemory(device, memory, NULL);
  }

  renderer->garbage_memory_counts[collect] = 0;

  for (i = 0; i < renderer->garbage_buffer_counts[collect]; ++i) {
    VkBuffer buffer = renderer->garbage_buffers[collect][i];
    vkDestroyBuffer(device, buffer, NULL);
  }

  renderer->garbage_buffer_counts[collect] = 0;
}

static void owl_renderer_deinit_garbage(struct owl_renderer *renderer) {
  renderer->garbage = 0;
  for (; renderer->garbage < OWL_GARBAGE_FRAME_COUNT; ++renderer->garbage)
    owl_renderer_collect_garbage(renderer);
}

static owl_code
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

static owl_code
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

static owl_code
owl_renderer_garbage_push_uniform(struct owl_renderer *renderer) {
  uint32_t i;
  uint32_t const capacity = OWL_ARRAY_SIZE(renderer->garbage_buffers[0]);
  uint32_t const garbage = renderer->garbage;
  uint32_t const buffer_count = renderer->garbage_buffer_counts[garbage];
  uint32_t const memory_count = renderer->garbage_memory_counts[garbage];
  uint32_t descriptor_set_count =
      renderer->garbage_descriptor_set_counts[garbage];

  if (capacity <= buffer_count + renderer->frame_count)
    return OWL_ERROR_NO_SPACE;

  if (capacity <= memory_count + 1)
    return OWL_ERROR_NO_SPACE;

  if (capacity <= descriptor_set_count + renderer->frame_count * 3)
    return OWL_ERROR_NO_SPACE;

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

  for (i = 0; i < OWL_IN_FLIGHT_FRAME_COUNT; ++i) {
    uint32_t const count = descriptor_set_count;
    VkDescriptorSet descriptor_set;

    descriptor_set = renderer->uniform_model_descriptor_sets[i];
    renderer->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
  }
  descriptor_set_count += renderer->frame_count;
  renderer->garbage_descriptor_set_counts[garbage] = descriptor_set_count;

  return OWL_OK;
}

static owl_code owl_renderer_init_vertex_buffer(struct owl_renderer *renderer,
                                                uint64_t size) {
  int32_t i;

  VkDevice const device = renderer->device;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(device, &info, NULL,
                               &renderer->vertex_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits properties;
    VkMemoryAllocateInfo info;
    VkMemoryRequirements requirements;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device, renderer->vertex_buffers[0],
                                  &requirements);

    renderer->vertex_buffer_alignment = requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
    renderer->vertex_buffer_aligned_size = aligned_size;

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = aligned_size * renderer->frame_count;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL,
                                 &renderer->vertex_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->vertex_buffer_memory;
      VkBuffer buffer = renderer->vertex_buffers[j];
      vk_result = vkBindBufferMemory(device, buffer, memory, aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(device, renderer->vertex_buffer_memory, 0, size, 0,
                            &renderer->vertex_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  renderer->vertex_buffer_offset = 0;
  renderer->vertex_buffer_size = size;

  return OWL_OK;

error_free_memory:
  vkFreeMemory(device, renderer->vertex_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(device, renderer->vertex_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_vertex_buffer(struct owl_renderer *renderer) {
  uint32_t i;
  VkDevice const device = renderer->device;

  vkFreeMemory(device, renderer->vertex_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(device, renderer->vertex_buffers[i], NULL);
}

static owl_code owl_renderer_init_index_buffer(struct owl_renderer *renderer,
                                               uint64_t size) {
  int32_t i;
  VkDevice const device = renderer->device;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(device, &info, NULL,
                               &renderer->index_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits properties;
    VkMemoryAllocateInfo info;
    VkMemoryRequirements requirements;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device, renderer->index_buffers[0],
                                  &requirements);

    renderer->index_buffer_alignment = requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
    renderer->index_buffer_aligned_size = aligned_size;

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = aligned_size * renderer->frame_count;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL,
                                 &renderer->index_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->index_buffer_memory;
      VkBuffer buffer = renderer->index_buffers[j];
      vk_result = vkBindBufferMemory(device, buffer, memory, aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(device, renderer->index_buffer_memory, 0, size, 0,
                            &renderer->index_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  renderer->index_buffer_offset = 0;
  renderer->index_buffer_size = size;

  return OWL_OK;

error_free_memory:
  vkFreeMemory(device, renderer->index_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(device, renderer->index_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_index_buffer(struct owl_renderer *renderer) {
  uint32_t i;
  VkDevice const device = renderer->device;

  vkFreeMemory(device, renderer->index_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(device, renderer->index_buffers[i], NULL);
}

static owl_code owl_renderer_init_uniform_buffer(struct owl_renderer *renderer,
                                                 uint64_t size) {
  int32_t i;
  VkDevice const device = renderer->device;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkBufferCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(device, &info, NULL,
                               &renderer->uniform_buffers[i]);
    if (vk_result)
      goto error_destroy_buffers;
  }

  {
    uint32_t j;
    VkDeviceSize aligned_size;
    VkMemoryPropertyFlagBits properties;
    VkMemoryAllocateInfo info;
    VkMemoryRequirements requirements;
    VkResult vk_result = VK_SUCCESS;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device, renderer->uniform_buffers[0],
                                  &requirements);

    renderer->uniform_buffer_alignment = requirements.alignment;
    aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
    renderer->uniform_buffer_aligned_size = aligned_size;

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = aligned_size * renderer->frame_count;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL,
                                 &renderer->uniform_buffer_memory);
    if (vk_result)
      goto error_destroy_buffers;

    for (j = 0; j < renderer->frame_count; ++j) {
      VkDeviceMemory memory = renderer->uniform_buffer_memory;
      VkBuffer buffer = renderer->uniform_buffers[j];
      vk_result = vkBindBufferMemory(device, buffer, memory, aligned_size * j);
      if (vk_result)
        goto error_free_memory;
    }

    vk_result = vkMapMemory(device, renderer->uniform_buffer_memory, 0, size,
                            0, &renderer->uniform_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    {
      VkDescriptorSetLayout layouts[2];
      VkDescriptorSet descriptor_sets[2];
      VkDescriptorSetAllocateInfo info;
      VkResult vk_result = VK_SUCCESS;

      layouts[0] = renderer->common_uniform_descriptor_set_layout;
      layouts[1] = renderer->model_uniform_descriptor_set_layout;

      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      info.pNext = NULL;
      info.descriptorPool = renderer->descriptor_pool;
      info.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
      info.pSetLayouts = layouts;

      vk_result = vkAllocateDescriptorSets(device, &info, descriptor_sets);
      if (vk_result)
        goto error_free_descriptor_sets;

      renderer->uniform_pvm_descriptor_sets[i] = descriptor_sets[0];
      renderer->uniform_model_descriptor_sets[i] = descriptor_sets[1];
    }

    {
      VkDescriptorBufferInfo descriptors[2];
      VkWriteDescriptorSet writes[2];

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
      descriptors[1].range = sizeof(struct owl_model_uniform);

      writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[1].pNext = NULL;
      writes[1].dstSet = renderer->uniform_model_descriptor_sets[i];
      writes[1].dstBinding = 0;
      writes[1].dstArrayElement = 0;
      writes[1].descriptorCount = 1;
      writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      writes[1].pImageInfo = NULL;
      writes[1].pBufferInfo = &descriptors[1];
      writes[1].pTexelBufferView = NULL;

      vkUpdateDescriptorSets(device, OWL_ARRAY_SIZE(writes), writes, 0, NULL);
    }
  }

  renderer->uniform_buffer_offset = 0;
  renderer->uniform_buffer_size = size;

  return OWL_OK;

error_free_descriptor_sets:
  for (i = i - 1; i >= 0; --i) {
    VkDescriptorSet descriptor_sets[3];

    descriptor_sets[0] = renderer->uniform_pvm_descriptor_sets[i];
    descriptor_sets[1] = renderer->uniform_model_descriptor_sets[i];

    vkFreeDescriptorSets(device, renderer->descriptor_pool,
                         OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
  }

  i = renderer->frame_count;

error_free_memory:
  vkFreeMemory(device, renderer->uniform_buffer_memory, NULL);

  i = renderer->frame_count;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(device, renderer->uniform_buffers[i], NULL);

  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_uniform_buffer(struct owl_renderer *renderer) {
  uint32_t i;
  VkDevice const device = renderer->device;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkDescriptorSet descriptor_sets[2];

    descriptor_sets[0] = renderer->uniform_pvm_descriptor_sets[i];
    descriptor_sets[1] = renderer->uniform_model_descriptor_sets[i];

    vkFreeDescriptorSets(device, renderer->descriptor_pool,
                         OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
  }

  vkFreeMemory(device, renderer->uniform_buffer_memory, NULL);

  for (i = 0; i < renderer->frame_count; ++i)
    vkDestroyBuffer(device, renderer->uniform_buffers[i], NULL);
}

static owl_code owl_renderer_init_frames(struct owl_renderer *renderer) {
  int32_t i;
  VkDevice const device = renderer->device;

  renderer->frame = 0;

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkCommandPoolCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.queueFamilyIndex = renderer->graphics_family;

    vk_result = vkCreateCommandPool(device, &info, NULL,
                                    &renderer->submit_command_pools[i]);
    if (vk_result)
      goto error_destroy_submit_command_pools;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkCommandBufferAllocateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = renderer->submit_command_pools[i];
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(device, &info,
                                         &renderer->submit_command_buffers[i]);
    if (vk_result)
      goto error_free_submit_command_buffers;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkFenceCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence(device, &info, NULL,
                              &renderer->in_flight_fences[i]);
    if (vk_result)
      goto error_destroy_in_flight_fences;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkSemaphoreCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vk_result = vkCreateSemaphore(device, &info, NULL,
                                  &renderer->acquire_semaphores[i]);
    if (vk_result)
      goto error_destroy_acquire_semaphores;
  }

  for (i = 0; i < (int32_t)renderer->frame_count; ++i) {
    VkSemaphoreCreateInfo info;
    VkResult vk_result = VK_SUCCESS;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vk_result = vkCreateSemaphore(device, &info, NULL,
                                  &renderer->render_done_semaphores[i]);
    if (vk_result)
      goto error_destroy_render_done_semaphores;
  }

  return OWL_OK;

error_destroy_render_done_semaphores:
  for (i = i - 1; i > 0; --i) {
    VkSemaphore semaphore = renderer->render_done_semaphores[i];
    vkDestroySemaphore(device, semaphore, NULL);
  }

  i = renderer->frame_count;

error_destroy_acquire_semaphores:
  for (i = i - 1; i > 0; --i) {
    VkSemaphore semaphore = renderer->acquire_semaphores[i];
    vkDestroySemaphore(device, semaphore, NULL);
  }

  i = renderer->frame_count;

error_destroy_in_flight_fences:
  for (i = i - 1; i > 0; --i) {
    VkFence fence = renderer->in_flight_fences[i];
    vkDestroyFence(device, fence, NULL);
  }

  i = renderer->frame_count;

error_free_submit_command_buffers:
  for (i = i - 1; i > 0; --i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    VkCommandBuffer command_buffer = renderer->submit_command_buffers[i];
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
  }

error_destroy_submit_command_pools:
  for (i = i - 1; i > 0; --i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    vkDestroyCommandPool(device, command_pool, NULL);
  }

  return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_frames(struct owl_renderer *renderer) {
  uint32_t i;

  VkDevice const device = renderer->device;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkSemaphore semaphore = renderer->render_done_semaphores[i];
    vkDestroySemaphore(device, semaphore, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkSemaphore semaphore = renderer->acquire_semaphores[i];
    vkDestroySemaphore(device, semaphore, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkFence fence = renderer->in_flight_fences[i];
    vkDestroyFence(device, fence, NULL);
  }

  i = renderer->frame_count;

  for (i = 0; i < renderer->frame_count; ++i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    VkCommandBuffer command_buffer = renderer->submit_command_buffers[i];
    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
  }

  for (i = 0; i < renderer->frame_count; ++i) {
    VkCommandPool command_pool = renderer->submit_command_pools[i];
    vkDestroyCommandPool(device, command_pool, NULL);
  }
}

static owl_code owl_renderer_init_samplers(struct owl_renderer *renderer) {
  VkSamplerCreateInfo info;
  VkResult vk_result = VK_SUCCESS;
  VkDevice const device = renderer->device;

  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.magFilter = VK_FILTER_LINEAR;
  info.minFilter = VK_FILTER_LINEAR;
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.mipLodBias = 0.0F;
  info.anisotropyEnable = VK_TRUE;
  info.maxAnisotropy = 16.0F;
  info.compareEnable = VK_FALSE;
  info.compareOp = VK_COMPARE_OP_ALWAYS;
  info.minLod = 0.0F;
  info.maxLod = VK_LOD_CLAMP_NONE;
  info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  info.unnormalizedCoordinates = VK_FALSE;

  vk_result = vkCreateSampler(device, &info, NULL, &renderer->linear_sampler);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

static void owl_renderer_deinit_samplers(struct owl_renderer *renderer) {
  VkDevice const device = renderer->device;
  vkDestroySampler(device, renderer->linear_sampler, NULL);
}

OWL_PUBLIC owl_code owl_renderer_init(struct owl_renderer *renderer,
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
  renderer->frame_count = OWL_IN_FLIGHT_FRAME_COUNT;

  renderer->clear_values[0].color.float32[0] = 0.01F;
  renderer->clear_values[0].color.float32[1] = 0.01F;
  renderer->clear_values[0].color.float32[2] = 0.01F;
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

  code = owl_renderer_select_physical_device(renderer);
  if (code) {
    OWL_DEBUG_LOG("Couldn't find physical device\n");
    goto error_deinit_surface;
  }

  code = owl_renderer_clamp_dimensions(renderer);
  if (code) {
    OWL_DEBUG_LOG("Couldn't clamp dimensions\n");
    goto error_deinit_surface;
  }

  code = owl_renderer_init_device(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize device!\n");
    goto error_deinit_surface;
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

  code = owl_renderer_init_graphics_pipelines(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize graphics pipelines!\n");
    goto error_deinit_layouts;
  }

  code = owl_renderer_init_compute_pipelines(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize compute pipelines!\n");
    goto error_deinit_graphics_pipelines;
  }

  code = owl_renderer_init_upload_buffer(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to initialize upload heap!\n");
    goto error_deinit_compute_pipelines;
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

  code = owl_renderer_init_vertex_buffer(renderer, OWL_DEFAULT_BUFFER_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Filed to initilize vertex buffer!\n");
    goto error_deinit_garbage;
  }

  code = owl_renderer_init_index_buffer(renderer, OWL_DEFAULT_BUFFER_SIZE);
  if (code) {
    OWL_DEBUG_LOG("Filed to initilize index buffer!\n");
    goto error_deinit_vertex_buffer;
  }

  code = owl_renderer_init_uniform_buffer(renderer, OWL_DEFAULT_BUFFER_SIZE);
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

error_deinit_compute_pipelines:
  owl_renderer_deinit_compute_pipelines(renderer);

error_deinit_graphics_pipelines:
  owl_renderer_deinit_graphics_pipelines(renderer);

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

OWL_PUBLIC void owl_renderer_deinit(struct owl_renderer *renderer) {
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
  owl_renderer_deinit_compute_pipelines(renderer);
  owl_renderer_deinit_graphics_pipelines(renderer);
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
    goto error;
  }

  owl_plataform_get_framebuffer_dimensions(renderer->plataform, &width,
                                           &height);
  renderer->width = width;
  renderer->height = height;

  ratio = (float)width / (float)height;
  owl_m4_perspective(fov, ratio, near, far, renderer->projection);

  owl_renderer_deinit_graphics_pipelines(renderer);
  owl_renderer_deinit_swapchain(renderer);
  owl_renderer_deinit_attachments(renderer);

  code = owl_renderer_clamp_dimensions(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to clamp dimensions!\n");
    goto error;
  }

  code = owl_renderer_init_attachments(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize attachments!\n");
    goto error;
  }

  code = owl_renderer_init_swapchain(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize swapchain!\n");
    goto error_deinit_attachments;
  }

  code = owl_renderer_init_graphics_pipelines(renderer);
  if (code) {
    OWL_DEBUG_LOG("Failed to resize pipelines!\n");
    goto error_deinit_swapchain;
  }

  return OWL_OK;

error_deinit_swapchain:
  owl_renderer_deinit_swapchain(renderer);

error_deinit_attachments:
  owl_renderer_deinit_attachments(renderer);

error:
  return code;
}

OWL_PUBLIC void *owl_renderer_vertex_allocate(
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

OWL_PUBLIC void *owl_renderer_uniform_allocate(
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
  allocation->common_descriptor_set =
      renderer->uniform_pvm_descriptor_sets[frame];
  allocation->model_descriptor_set =
      renderer->uniform_model_descriptor_sets[frame];

  renderer->uniform_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

  return &data[frame * aligned_size + allocation->offset];
}

#define OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result)                      \
  (VK_ERROR_OUT_OF_DATE_KHR == (vk_result) ||                                 \
   VK_SUBOPTIMAL_KHR == (vk_result) ||                                        \
   VK_ERROR_SURFACE_LOST_KHR == (vk_result))

OWL_PUBLIC owl_code owl_renderer_begin_frame(struct owl_renderer *renderer) {
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  uint64_t const timeout = (uint64_t)-1;
  uint32_t const clear_value_count = OWL_ARRAY_SIZE(renderer->clear_values);
  uint32_t const frame = renderer->frame;
  VkClearValue const *clear_values = renderer->clear_values;
  VkCommandBuffer command_buffer = renderer->submit_command_buffers[frame];
  VkSemaphore acquire_semaphore = renderer->acquire_semaphores[frame];
  VkDevice const device = renderer->device;

  vk_result = vkAcquireNextImageKHR(device, renderer->swapchain, timeout,
                                    acquire_semaphore, VK_NULL_HANDLE,
                                    &renderer->swapchain_image);
  if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result)) {
    code = owl_renderer_resize_swapchain(renderer);
    if (code)
      return code;

    vk_result = vkAcquireNextImageKHR(device, renderer->swapchain, timeout,
                                      acquire_semaphore, VK_NULL_HANDLE,
                                      &renderer->swapchain_image);
    if (vk_result)
      return OWL_ERROR_FATAL;
  }

  {
    VkFence in_flight_fence = renderer->in_flight_fences[frame];
    VkCommandPool command_pool = renderer->submit_command_pools[frame];

    vk_result = vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, timeout);
    if (vk_result)
      return OWL_ERROR_FATAL;

    vk_result = vkResetFences(device, 1, &in_flight_fence);
    if (vk_result)
      return OWL_ERROR_FATAL;

    vk_result = vkResetCommandPool(device, command_pool, 0);
    if (vk_result)
      return OWL_ERROR_FATAL;
  }

  owl_renderer_collect_garbage(renderer);

  {
    VkCommandBufferBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    vk_result = vkBeginCommandBuffer(command_buffer, &info);
    if (vk_result)
      return OWL_ERROR_FATAL;
  }

  {
    VkRenderPassBeginInfo info;
    VkFramebuffer framebuffer;

    framebuffer = renderer->swapchain_framebuffers[renderer->swapchain_image];
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext = NULL;
    info.renderPass = renderer->main_render_pass;
    info.framebuffer = framebuffer;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent.width = renderer->width;
    info.renderArea.extent.height = renderer->height;
    info.clearValueCount = clear_value_count;
    info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  return OWL_OK;
}

OWL_PUBLIC owl_code owl_renderer_end_frame(struct owl_renderer *renderer) {
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

  {
    VkSubmitInfo info;
    VkPipelineStageFlagBits stage;

    stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext = NULL;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &acquire_semaphore;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &render_done_semaphore;
    info.pWaitDstStageMask = &stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &command_buffer;

    vk_result = vkQueueSubmit(renderer->graphics_queue, 1, &info,
                              in_flight_fence);
    if (vk_result)
      return OWL_ERROR_FATAL;
  }

  {
    VkPresentInfoKHR info;

    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.pNext = NULL;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_done_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &renderer->swapchain;
    info.pImageIndices = &renderer->swapchain_image;
    info.pResults = NULL;

    vk_result = vkQueuePresentKHR(renderer->present_queue, &info);
    if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result))
      return owl_renderer_resize_swapchain(renderer);
  }

  renderer->frame = (renderer->frame + 1) % renderer->frame_count;
  renderer->vertex_buffer_offset = 0;
  renderer->index_buffer_offset = 0;
  renderer->uniform_buffer_offset = 0;

  return OWL_OK;
}

OWL_PUBLIC void *owl_renderer_upload_allocate(
    struct owl_renderer *renderer, uint64_t size,
    struct owl_renderer_upload_allocation *allocation) {
  VkResult vk_result;
  VkDevice const device = renderer->device;

  if (renderer->upload_buffer_in_use)
    goto error;

  renderer->upload_buffer_size = size;

  {
    VkBufferCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    vk_result = vkCreateBuffer(device, &info, NULL, &renderer->upload_buffer);
    if (vk_result)
      goto error;
  }

  {
    VkMemoryPropertyFlagBits properties;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    properties = 0;
    properties |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    properties |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vkGetBufferMemoryRequirements(device, renderer->upload_buffer,
                                  &requirements);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = requirements.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type(
        renderer, requirements.memoryTypeBits, properties);

    vk_result = vkAllocateMemory(device, &info, NULL,
                                 &renderer->upload_buffer_memory);
    if (vk_result)
      goto error_destroy_buffer;

    vk_result = vkBindBufferMemory(device, renderer->upload_buffer,
                                   renderer->upload_buffer_memory, 0);
    if (vk_result)
      goto error_free_memory;

    vk_result = vkMapMemory(device, renderer->upload_buffer_memory, 0,
                            renderer->upload_buffer_size, 0,
                            &renderer->upload_buffer_data);
    if (vk_result)
      goto error_free_memory;
  }

  allocation->buffer = renderer->upload_buffer;

  return renderer->upload_buffer_data;

error_free_memory:
  vkFreeMemory(device, renderer->upload_buffer_memory, NULL);

error_destroy_buffer:
  vkDestroyBuffer(device, renderer->upload_buffer, NULL);

error:
  return NULL;
}

OWL_PUBLIC void owl_renderer_upload_free(struct owl_renderer *renderer,
                                         void *ptr) {
  VkDevice const device = renderer->device;

  OWL_UNUSED(ptr);
  OWL_ASSERT(renderer->upload_buffer_data == ptr);
  renderer->upload_buffer_in_use = 0;
  renderer->upload_buffer_size = 0;
  renderer->upload_buffer_data = NULL;

  vkFreeMemory(device, renderer->upload_buffer_memory, NULL);
  vkDestroyBuffer(device, renderer->upload_buffer, NULL);
}

#define OWL_FONT_ATLAS_WIDTH 1024
#define OWL_FONT_ATLAS_HEIGHT 1024
#define OWL_FONT_ATLAS_SIZE (OWL_FONT_ATLAS_HEIGHT * OWL_FONT_ATLAS_WIDTH)

OWL_PUBLIC owl_code owl_renderer_load_font(struct owl_renderer *renderer,
                                           uint32_t size, char const *path) {
  owl_code code;
  uint8_t *bitmap;
  stbtt_pack_context pack;
  stbtt_packedchar *packed_chars;
  struct owl_plataform_file file;
  struct owl_texture_desc texture_desc;
  int32_t stb_result;
  int32_t const width = OWL_FONT_ATLAS_WIDTH;
  int32_t const height = OWL_FONT_ATLAS_HEIGHT;
  int32_t const first = OWL_FIRST_CHAR;
  int32_t const num = OWL_CHAR_COUNT;

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

  packed_chars = (stbtt_packedchar *)(&renderer->font_chars[0]);

  stb_result = stbtt_PackFontRange(&pack, file.data, 0, size, first, num,
                                   packed_chars);
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

OWL_PUBLIC void owl_renderer_unload_font(struct owl_renderer *renderer) {
  owl_texture_deinit(renderer, &renderer->font_atlas);
  renderer->font_loaded = 0;
}

#define OWL_MAX_SKYBOX_PATH_LENGTH 256

OWL_PUBLIC owl_code owl_renderer_load_skybox(struct owl_renderer *renderer,
                                             char const *path) {
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

OWL_PUBLIC void owl_renderer_unload_skybox(struct owl_renderer *renderer) {
  owl_texture_deinit(renderer, &renderer->skybox);
  renderer->skybox_loaded = 0;
}

OWL_PUBLIC uint32_t owl_renderer_find_memory_type(
    struct owl_renderer *renderer, uint32_t filter, uint32_t properties) {
  uint32_t type;
  VkPhysicalDeviceMemoryProperties memory_properties;
  VkPhysicalDevice const physical_device = renderer->physical_device;

  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

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
  VkDevice const device = renderer->device;

  OWL_ASSERT(!renderer->immediate_command_buffer);

  {
    VkCommandBufferAllocateInfo info;
    VkCommandBuffer *command_buffer = &renderer->immediate_command_buffer;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = renderer->command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(device, &info, command_buffer);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkCommandBufferBeginInfo info;
    VkCommandBuffer command_buffer = renderer->immediate_command_buffer;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    vk_result = vkBeginCommandBuffer(command_buffer, &info);
    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_immediate_command_buffer_deinit;
    }
  }

  goto out;

error_immediate_command_buffer_deinit:
  vkFreeCommandBuffers(device, renderer->command_pool, 1,
                       &renderer->immediate_command_buffer);

out:
  return code;
}

OWL_PUBLIC owl_code
owl_renderer_end_immediate_command_buffer(struct owl_renderer *renderer) {
  VkSubmitInfo info;
  VkResult vk_result = VK_SUCCESS;
  owl_code code = OWL_OK;

  OWL_ASSERT(renderer->immediate_command_buffer);

  vk_result = vkEndCommandBuffer(renderer->immediate_command_buffer);
  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = NULL;
  info.waitSemaphoreCount = 0;
  info.pWaitSemaphores = NULL;
  info.pWaitDstStageMask = NULL;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &renderer->immediate_command_buffer;
  info.signalSemaphoreCount = 0;
  info.pSignalSemaphores = NULL;

  vk_result = vkQueueSubmit(renderer->graphics_queue, 1, &info, NULL);
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

OWL_PUBLIC owl_code owl_renderer_fill_glyph(struct owl_renderer *renderer,
                                            char c, owl_v2 offset,
                                            struct owl_glyph *glyph) {
  stbtt_aligned_quad quad;
  owl_code code = OWL_OK;

  stbtt_GetPackedQuad((stbtt_packedchar *)(&renderer->font_chars[0]),
                      OWL_FONT_ATLAS_WIDTH, OWL_FONT_ATLAS_HEIGHT,
                      c - OWL_FIRST_CHAR, &offset[0], &offset[1], &quad, 1);

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

#include "owl-vk-init.h"

#include "owl-internal.h"
#include "owl-model.h"
#include "owl-plataform.h"
#include "owl-vk-misc.h"
#include "owl-vk-pipeline.h"
#include "owl-vk-renderer.h"
#include "owl-vk-types.h"

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32
owl_vk_debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user)
{

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

owl_public enum owl_code
owl_vk_init_instance(struct owl_vk_renderer *vk,
                     struct owl_plataform *plataform)
{
  VkApplicationInfo application_info;
  VkInstanceCreateInfo instance_info;

#if defined(OWL_ENABLE_VALIDATION)
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info;
#endif

  owl_u32 num_extensions;
  char const *const *extensions;

  VkResult vk_result;
  enum owl_code code;

  code = owl_plataform_get_vulkan_extensions(plataform, &num_extensions,
                                             &extensions);
  if (code)
    return code;

  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = NULL;
  application_info.pApplicationName = owl_plataform_get_title(plataform);
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
  inst_info.enabledLayerCount = 0;
  inst_info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
  instance_info.enabledExtensionCount = num_extensions;
  instance_info.ppEnabledExtensionNames = extensions;

  vk_result = vkCreateInstance(&instance_info, NULL, &vk->instance);
  if (vk_result)
    return OWL_ERROR_FATAL;

#if defined(OWL_ENABLE_VALIDATION)
  vk->vk_create_debug_utils_messenger_ext =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          vk->instance, "vkCreateDebugUtilsMessengerEXT");
  if (!vk->vk_create_debug_utils_messenger_ext)
    goto error_destroy_instance;

  vk->vk_destroy_debug_utils_messenger_ext =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          vk->instance, "vkDestroyDebugUtilsMessengerEXT");
  if (!vk->vk_destroy_debug_utils_messenger_ext)
    goto error_destroy_instance;

  debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_messenger_info.pNext = NULL;
  debug_messenger_info.flags = 0;
  debug_messenger_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_messenger_info.pfnUserCallback = owl_vk_debug_messenger_callback;
  debug_messenger_info.pUserData = vk;

  vk_result = vk->vk_create_debug_utils_messenger_ext(
      vk->instance, &debug_messenger_info, NULL, &vk->debug_messenger);
  if (vk_result)
    goto error_destroy_instance;
#endif

  return OWL_OK;

#if defined(OWL_ENABLE_VALIDATION)
error_destroy_instance:
  vkDestroyInstance(vk->instance, NULL);

  return OWL_ERROR_FATAL;
#endif
}

owl_public void
owl_vk_deinit_instance(struct owl_vk_renderer *vk)
{
#if defined(OWL_ENABLE_VALIDATION)
  vk->vk_destroy_debug_utils_messenger_ext(vk->instance, vk->debug_messenger,
                                           NULL);
#endif
  vkDestroyInstance(vk->instance, NULL);
}

owl_public enum owl_code
owl_vk_init_surface(struct owl_vk_renderer *vk,
                    struct owl_plataform *plataform)
{
  return owl_plataform_create_vulkan_surface(plataform, vk);
}

owl_public void
owl_vk_deinit_surface(struct owl_vk_renderer *vk)
{
  vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
}

owl_private char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

owl_private owl_b32
owl_vk_validate_queue_families(struct owl_vk_renderer *vk)
{
  if ((owl_u32)-1 == vk->graphics_queue_family)
    return 0;

  if ((owl_u32)-1 == vk->present_queue_family)
    return 0;

  return 1;
}

owl_private owl_b32
owl_vk_ensure_queue_families(struct owl_vk_renderer *vk)
{
  owl_u32 i;
  owl_u32 num_properties = 0;
  VkQueueFamilyProperties *properties = NULL;

  VkResult vk_result;

  vk->graphics_queue_family = (owl_u32)-1;
  vk->present_queue_family = (owl_u32)-1;

  vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device,
                                           &num_properties, NULL);

  properties = owl_malloc(num_properties * sizeof(*properties));
  if (!properties)
    return 0;

  vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device,
                                           &num_properties, properties);

  for (i = 0; i < num_properties; ++i) {
    VkBool32 has_surf;

    vk_result = vkGetPhysicalDeviceSurfaceSupportKHR(vk->physical_device, i,
                                                     vk->surface, &has_surf);
    if (vk_result)
      goto cleanup;

    if (has_surf)
      vk->present_queue_family = i;

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags)
      vk->graphics_queue_family = i;

    if (owl_vk_validate_queue_families(vk))
      goto cleanup;
  }

cleanup:
  if (properties)
    owl_free(properties);

  return owl_vk_validate_queue_families(vk);
}

owl_private owl_b32
owl_vk_validate_device_extensions(struct owl_vk_renderer *vk)
{
  owl_b32 found = 0;
  owl_u32 i = 0;
  owl_u32 num_properties = 0;
  VkExtensionProperties *properties = NULL;
  owl_b32 *state = NULL;
  VkResult vk_result = VK_SUCCESS;

  vk_result = vkEnumerateDeviceExtensionProperties(vk->physical_device, NULL,
                                                   &num_properties, NULL);
  if (vk_result)
    return 0;

  properties = owl_malloc(num_properties * sizeof(*properties));
  if (!properties)
    return 0;

  vk_result = vkEnumerateDeviceExtensionProperties(
      vk->physical_device, NULL, &num_properties, properties);
  if (vk_result)
    goto cleanup;

  state = owl_malloc(owl_array_size(device_extensions) * sizeof(*state));
  if (!state)
    goto cleanup;

  for (i = 0; i < owl_array_size(device_extensions); ++i)
    state[i] = 0;

  for (i = 0; i < num_properties; ++i) {
    owl_u32 j;
    for (j = 0; j < owl_array_size(device_extensions); ++j) {
      char const *l = device_extensions[j];
      char const *r = properties[j].extensionName;

      owl_u32 minlen = owl_min(VK_MAX_EXTENSION_NAME_SIZE, owl_strlen(l));

      state[j] = !owl_strncmp(l, r, minlen);
    }
  }

  found = 1;
  for (i = 0; i < owl_array_size(device_extensions) && found; ++i)
    if (!state[i])
      found = 0;

cleanup:
  if (state)
    owl_free(state);

  if (properties)
    owl_free(properties);

  return found;
}

owl_private enum owl_code
owl_vk_ensure_surface_format(struct owl_vk_renderer *vk, VkFormat fmt,
                             VkColorSpaceKHR color_space)
{
  owl_u32 i;
  owl_u32 num_fmts = 0;
  VkSurfaceFormatKHR *fmts = NULL;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_ERROR_NOT_FOUND;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      vk->physical_device, vk->surface, &num_fmts, NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  fmts = owl_malloc(num_fmts * sizeof(*fmts));
  if (!fmts)
    return OWL_ERROR_NO_MEMORY;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      vk->physical_device, vk->surface, &num_fmts, fmts);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  for (i = 0; i < num_fmts && OWL_ERROR_NOT_FOUND == code; ++i)
    if (fmts[i].format == fmt && fmts[i].colorSpace == color_space)
      code = OWL_OK;

  if (!code) {
    vk->surface_format.format = fmt;
    vk->surface_format.colorSpace = color_space;
  }

cleanup:
  if (fmts)
    owl_free(fmts);

  return code;
}

owl_private enum owl_code
owl_vk_ensure_msaa(struct owl_vk_renderer *vk, VkSampleCountFlagBits msaa)
{
  VkPhysicalDeviceProperties properties;
  VkSampleCountFlagBits limit = ~0U;

  enum owl_code code = OWL_OK;

  vkGetPhysicalDeviceProperties(vk->physical_device, &properties);

  limit &= properties.limits.framebufferColorSampleCounts;
  limit &= properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_1_BIT & msaa) {
    code = OWL_ERROR_NOT_FOUND;
  } else if (limit & msaa) {
    vk->msaa = msaa;
    code = OWL_OK;
  } else {
    vk->msaa = VK_SAMPLE_COUNT_2_BIT;
    code = OWL_ERROR_NOT_FOUND;
  }

  return code;
}

owl_private owl_b32
owl_vk_validate_physical_device(struct owl_vk_renderer *vk)
{
  owl_b32 has_fams;
  owl_b32 has_ext_props;
  owl_u32 has_fmts;
  owl_u32 has_modes;
  VkPhysicalDeviceFeatures feats;

  VkResult vk_result;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      vk->physical_device, vk->surface, &has_fmts, NULL);
  if (vk_result || !has_fmts)
    return 0;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      vk->physical_device, vk->surface, &has_modes, NULL);
  if (vk_result || !has_modes)
    return 0;

  has_fams = owl_vk_ensure_queue_families(vk);
  if (!has_fams)
    return 0;

  has_ext_props = owl_vk_validate_device_extensions(vk);
  if (!has_ext_props)
    return 0;

  vkGetPhysicalDeviceFeatures(vk->physical_device, &feats);
  if (!feats.samplerAnisotropy)
    return 0;

  return 1;
}

owl_private enum owl_code
owl_vk_ensure_physical_device(struct owl_vk_renderer *vk)
{
  owl_u32 i;
  owl_u32 num_physical_devices = 0;
  VkPhysicalDevice *physical_devices = NULL;
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  vk_result = vkEnumeratePhysicalDevices(vk->instance, &num_physical_devices,
                                         NULL);
  if (vk_result)
    return OWL_ERROR_FATAL;

  physical_devices =
      owl_malloc(num_physical_devices * sizeof(*physical_devices));

  if (!physical_devices)
    return OWL_ERROR_NO_MEMORY;

  vk_result = vkEnumeratePhysicalDevices(vk->instance, &num_physical_devices,
                                         physical_devices);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

  for (i = 0; i < num_physical_devices; ++i) {
    vk->physical_device = physical_devices[i];

    if (owl_vk_validate_physical_device(vk))
      break;
  }

  if (!owl_vk_validate_queue_families(vk)) {
    code = OWL_ERROR_FATAL;
    goto cleanup;
  }

cleanup:
  if (physical_devices)
    owl_free(physical_devices);

  return code;
}

owl_public enum owl_code
owl_vk_init_device(struct owl_vk_renderer *vk)
{

  VkPhysicalDeviceFeatures features;
  VkDeviceCreateInfo device_info;
  VkDeviceQueueCreateInfo queue_infos[2];
  float const priority = 1.0F;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  code = owl_vk_ensure_physical_device(vk);
  if (code)
    return code;

  code = owl_vk_ensure_msaa(vk, VK_SAMPLE_COUNT_2_BIT);
  if (code)
    return code;

  code = owl_vk_ensure_surface_format(vk, VK_FORMAT_B8G8R8A8_SRGB,
                                      VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
  if (code)
    return code;

  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = NULL;
  queue_infos[0].flags = 0;
  queue_infos[0].queueFamilyIndex = vk->graphics_queue_family;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &priority;

  queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[1].pNext = NULL;
  queue_infos[1].flags = 0;
  queue_infos[1].queueFamilyIndex = vk->present_queue_family;
  queue_infos[1].queueCount = 1;
  queue_infos[1].pQueuePriorities = &priority;

  vkGetPhysicalDeviceFeatures(vk->physical_device, &features);

  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = NULL;
  device_info.flags = 0;
  if (vk->graphics_queue_family == vk->present_queue_family) {
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

  vk_result = vkCreateDevice(vk->physical_device, &device_info, NULL,
                             &vk->device);
  if (vk_result)
    return OWL_ERROR_FATAL;

  vkGetDeviceQueue(vk->device, vk->graphics_queue_family, 0,
                   &vk->graphics_queue);

  vkGetDeviceQueue(vk->device, vk->present_queue_family, 0,
                   &vk->present_queue);

  return code;
}

owl_private enum owl_code
owl_vk_ensure_depth_format(struct owl_vk_renderer *vk, VkFormat format)
{
  VkFormatProperties props;

  enum owl_code code = OWL_ERROR_NOT_FOUND;

  vkGetPhysicalDeviceFormatProperties(vk->physical_device, format, &props);

  if (props.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    vk->depth_format = format;
    code = OWL_OK;
  }

  return code;
}

owl_public void
owl_vk_deinit_device(struct owl_vk_renderer *vk)
{
  vkDestroyDevice(vk->device, NULL);
}

owl_public enum owl_code
owl_vk_init_attachments(struct owl_vk_renderer *vk, owl_u32 w, owl_u32 h)
{
  VkImageCreateInfo image_info;
  VkMemoryRequirements memory_requirements;
  VkMemoryAllocateInfo memory_info;
  VkImageViewCreateInfo image_view_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  code = owl_vk_ensure_depth_format(vk, VK_FORMAT_D24_UNORM_S8_UINT);
  if (code) {
    code = owl_vk_ensure_depth_format(vk, VK_FORMAT_D32_SFLOAT_S8_UINT);
    if (code) {
      goto out;
    }
  }

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.flags = 0;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = vk->surface_format.format;
  image_info.extent.width = w;
  image_info.extent.height = h;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = vk->msaa;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result = vkCreateImage(vk->device, &image_info, NULL, &vk->color_image);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetImageMemoryRequirements(vk->device, vk->color_image,
                               &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex =
      owl_vk_find_memory_type(vk, memory_requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(vk->device, &memory_info, NULL,
                               &vk->color_memory);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_color_image;
  }

  vk_result = vkBindImageMemory(vk->device, vk->color_image, vk->color_memory,
                                0);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_depth_memory;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = vk->color_image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = vk->surface_format.format;
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  vk_result = vkCreateImageView(vk->device, &image_view_info, NULL,
                                &vk->color_image_view);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_color_memory;
  }

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.flags = 0;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = vk->depth_format;
  image_info.extent.width = w;
  image_info.extent.height = h;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = vk->msaa;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result = vkCreateImage(vk->device, &image_info, NULL, &vk->depth_image);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_color_image_view;
  }

  vkGetImageMemoryRequirements(vk->device, vk->depth_image,
                               &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex =
      owl_vk_find_memory_type(vk, memory_requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_result = vkAllocateMemory(vk->device, &memory_info, NULL, &vk->depth_mem);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_depth_image;
  }

  vk_result = vkBindImageMemory(vk->device, vk->depth_image, vk->depth_mem, 0);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_depth_memory;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = vk->depth_image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = vk->depth_format;
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  vk_result = vkCreateImageView(vk->device, &image_view_info, NULL,
                                &vk->depth_image_view);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_depth_memory;
  }

  goto out;

error_free_depth_memory:
  vkFreeMemory(vk->device, vk->depth_mem, NULL);

error_destroy_depth_image:
  vkDestroyImage(vk->device, vk->depth_image, NULL);

error_destroy_color_image_view:
  vkDestroyImageView(vk->device, vk->color_image_view, NULL);

error_free_color_memory:
  vkFreeMemory(vk->device, vk->color_memory, NULL);

error_destroy_color_image:
  vkDestroyImage(vk->device, vk->color_image, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_attachments(struct owl_vk_renderer *vk)
{
  vkDestroyImageView(vk->device, vk->depth_image_view, NULL);
  vkFreeMemory(vk->device, vk->depth_mem, NULL);
  vkDestroyImage(vk->device, vk->depth_image, NULL);
  vkDestroyImageView(vk->device, vk->color_image_view, NULL);
  vkFreeMemory(vk->device, vk->color_memory, NULL);
  vkDestroyImage(vk->device, vk->color_image, NULL);
}

owl_public enum owl_code
owl_vk_init_render_passes(struct owl_vk_renderer *vk)
{
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo pass_info;

  VkResult vk_result;

  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  resolve_reference.attachment = 2;
  resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  attachments[0].flags = 0;
  attachments[0].format = vk->surface_format.format;
  attachments[0].samples = vk->msaa;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  attachments[1].flags = 0;
  attachments[1].format = vk->depth_format;
  attachments[1].samples = vk->msaa;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  attachments[2].flags = 0;
  attachments[2].format = vk->surface_format.format;
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

  pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass_info.pNext = NULL;
  pass_info.flags = 0;
  pass_info.attachmentCount = owl_array_size(attachments);
  pass_info.pAttachments = attachments;
  pass_info.subpassCount = 1;
  pass_info.pSubpasses = &subpass;
  pass_info.dependencyCount = 1;
  pass_info.pDependencies = &dependency;

  vk_result = vkCreateRenderPass(vk->device, &pass_info, NULL,
                                 &vk->main_render_pass);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_public void
owl_vk_deinit_render_passes(struct owl_vk_renderer *vk)
{
  vkDestroyRenderPass(vk->device, vk->main_render_pass, NULL);
}

#define OWL_VK_MAX_PRESENT_MODES 16

owl_private enum owl_code
owl_vk_ensure_present_mode(struct owl_vk_renderer *vk, VkPresentModeKHR mode)
{
  owl_i32 i;
  owl_u32 num_modes;
  VkPresentModeKHR modes[OWL_VK_MAX_PRESENT_MODES];

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_ERROR_NOT_FOUND;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      vk->physical_device, vk->surface, &num_modes, NULL);
  if (vk_result || OWL_VK_MAX_PRESENT_MODES <= num_modes)
    return OWL_ERROR_FATAL;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
      vk->physical_device, vk->surface, &num_modes, modes);
  if (vk_result)
    return OWL_ERROR_FATAL;

  for (i = 0; i < (owl_i32)num_modes && OWL_ERROR_NOT_FOUND == code; ++i)
    if (modes[i] == mode)
      code = OWL_OK;

  if (!code) {
    vk->present_mode = mode;
  } else {
    vk->present_mode = VK_PRESENT_MODE_FIFO_KHR;
  }

  return code;
}

owl_public enum owl_code
owl_vk_init_swapchain(struct owl_vk_renderer *vk, owl_u32 w, owl_u32 h)
{
  owl_i32 i;
  owl_u32 families[2];
  VkSwapchainCreateInfoKHR swapchain_info;
  VkSurfaceCapabilitiesKHR capabilities;

  VkResult vk_result;
  enum owl_code code = OWL_OK;

  vk->swapchain_image = 0;

  vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      vk->physical_device, vk->surface, &capabilities);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  if ((owl_u32)-1 == capabilities.currentExtent.width) {
    vk->swapchain_width = w;
    vk->swapchain_height = h;
  } else {
    owl_u32 const min_width = capabilities.minImageExtent.width;
    owl_u32 const max_width = capabilities.maxImageExtent.width;
    owl_u32 const min_height = capabilities.minImageExtent.height;
    owl_u32 const max_height = capabilities.maxImageExtent.height;

    vk->swapchain_width = owl_clamp(w, min_width, max_width);
    vk->swapchain_height = owl_clamp(h, min_height, max_height);
  }

  code = owl_vk_ensure_present_mode(vk, VK_PRESENT_MODE_FIFO_KHR);
  if (code) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.pNext = NULL;
  swapchain_info.flags = 0;
  swapchain_info.surface = vk->surface;
  swapchain_info.minImageCount = OWL_VK_IN_FLIGHT_FRAMES;
  swapchain_info.imageFormat = vk->surface_format.format;
  swapchain_info.imageColorSpace = vk->surface_format.colorSpace;
  swapchain_info.imageExtent.width = vk->swapchain_width;
  swapchain_info.imageExtent.height = vk->swapchain_height;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_info.preTransform = capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = vk->present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  families[0] = vk->graphics_queue_family;
  families[1] = vk->present_queue_family;

  if (vk->graphics_queue_family == vk->present_queue_family) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = NULL;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = families;
  }

  vk_result = vkCreateSwapchainKHR(vk->device, &swapchain_info, NULL,
                                   &vk->swapchain);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vk_result = vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
                                      &vk->num_swapchain_images, NULL);
  if (vk_result || OWL_MAX_SWAPCHAIN_IMAGES <= vk->num_swapchain_images) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_swapchain;
  }

  vk_result = vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
                                      &vk->num_swapchain_images,
                                      vk->swapchain_images);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_swapchain;
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkImageViewCreateInfo image_info;

    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_info.pNext = NULL;
    image_info.flags = 0;
    image_info.image = vk->swapchain_images[i];
    image_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_info.format = vk->surface_format.format;
    image_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_info.subresourceRange.baseMipLevel = 0;
    image_info.subresourceRange.levelCount = 1;
    image_info.subresourceRange.baseArrayLayer = 0;
    image_info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView(vk->device, &image_info, NULL,
                                  &vk->swapchain_image_views[i]); /**/
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_views;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo framebuffer_info;

    attachments[0] = vk->color_image_view;
    attachments[1] = vk->depth_image_view;
    attachments[2] = vk->swapchain_image_views[i];

    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.pNext = NULL;
    framebuffer_info.flags = 0;
    framebuffer_info.renderPass = vk->main_render_pass;
    framebuffer_info.attachmentCount = owl_array_size(attachments);
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = vk->swapchain_width;
    framebuffer_info.height = vk->swapchain_height;
    framebuffer_info.layers = 1;

    vk_result = vkCreateFramebuffer(vk->device, &framebuffer_info, NULL,
                                    &vk->swapchain_framebuffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_framebuffers;
    }
  }

  goto out;

error_destroy_framebuffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFramebuffer(vk->device, vk->swapchain_framebuffers[i], NULL);

  i = vk->num_swapchain_images;

error_destroy_image_views:
  for (i = i - 1; i >= 0; --i)
    vkDestroyImageView(vk->device, vk->swapchain_image_views[i], NULL);

error_destroy_swapchain:
  vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_swapchain(struct owl_vk_renderer *vk)
{
  owl_u32 i;

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroyFramebuffer(vk->device, vk->swapchain_framebuffers[i], NULL);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroyImageView(vk->device, vk->swapchain_image_views[i], NULL);

  vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
}

owl_public enum owl_code
owl_vk_init_pools(struct owl_vk_renderer *vk)
{
  VkCommandPoolCreateInfo command_pool_info;
  VkDescriptorPoolSize sizes[6];
  VkDescriptorPoolCreateInfo descriptor_pool_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.pNext = NULL;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  command_pool_info.queueFamilyIndex = vk->graphics_queue_family;

  vk_result = vkCreateCommandPool(vk->device, &command_pool_info, NULL,
                                  &vk->command_pool);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

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

  vk_result = vkCreateDescriptorPool(vk->device, &descriptor_pool_info, NULL,
                                     &vk->descriptor_pool);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_command_pool;
  }

  goto out;

error_destroy_command_pool:
  vkDestroyCommandPool(vk->device, vk->command_pool, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_pools(struct owl_vk_renderer *vk)
{
  vkDestroyDescriptorPool(vk->device, vk->descriptor_pool, NULL);
  vkDestroyCommandPool(vk->device, vk->command_pool, NULL);
}

owl_public enum owl_code
owl_vk_init_shaders(struct owl_vk_renderer *vk)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  {
    VkShaderModuleCreateInfo info;

    /* TODO(samuel): Properly load code at runtime */
    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->basic_vertex_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->basic_fragment_shader);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_basic_vertex_shader;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_font.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->text_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_basic_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->model_vertex_shader);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_text_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_model.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->model_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_vertex_shader;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->skybox_vertex_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_fragment_shader;
    }
  }

  {
    VkShaderModuleCreateInfo info;

    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_skybox.frag.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof(spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule(vk->device, &info, NULL,
                                     &vk->skybox_fragment_shader);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_skybox_vertex_shader;
    }
  }

  goto out;

error_destroy_skybox_vertex_shader:
  vkDestroyShaderModule(vk->device, vk->skybox_vertex_shader, NULL);

error_destroy_model_fragment_shader:
  vkDestroyShaderModule(vk->device, vk->model_fragment_shader, NULL);

error_destroy_model_vertex_shader:
  vkDestroyShaderModule(vk->device, vk->model_vertex_shader, NULL);

error_destroy_text_fragment_shader:
  vkDestroyShaderModule(vk->device, vk->text_fragment_shader, NULL);

error_destroy_basic_fragment_shader:
  vkDestroyShaderModule(vk->device, vk->basic_fragment_shader, NULL);

error_destroy_basic_vertex_shader:
  vkDestroyShaderModule(vk->device, vk->basic_vertex_shader, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_shaders(struct owl_vk_renderer *vk)
{
  vkDestroyShaderModule(vk->device, vk->skybox_fragment_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->skybox_vertex_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->model_fragment_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->model_vertex_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->text_fragment_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->basic_fragment_shader, NULL);
  vkDestroyShaderModule(vk->device, vk->basic_vertex_shader, NULL);
}

owl_public enum owl_code
owl_vk_init_layouts(struct owl_vk_renderer *vk)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.pNext = NULL;
    descriptor_set_layout_info.flags = 0;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        vk->device, &descriptor_set_layout_info, NULL,
        &vk->ubo_vertex_descriptor_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto out;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.pNext = NULL;
    descriptor_set_layout_info.flags = 0;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        vk->device, &descriptor_set_layout_info, NULL,
        &vk->ubo_fragment_descriptor_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_vertex_descriptor_set_layout;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT |
                         VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.pNext = NULL;
    descriptor_set_layout_info.flags = 0;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    vk_result =
        vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_info,
                                    NULL, &vk->ubo_both_descriptor_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_fragment_descriptor_set_layout;
    }
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    descriptor_set_layout_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.pNext = NULL;
    descriptor_set_layout_info.flags = 0;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    vk_result = vkCreateDescriptorSetLayout(
        vk->device, &descriptor_set_layout_info, NULL,
        &vk->ssbo_vertex_descriptor_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ubo_both_descriptor_set_layout;
    }
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
    descriptor_set_layout_info.bindingCount = owl_array_size(bindings);
    descriptor_set_layout_info.pBindings = bindings;

    vk_result = vkCreateDescriptorSetLayout(
        vk->device, &descriptor_set_layout_info, NULL,
        &vk->image_fragment_descriptor_set_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_ssbo_vertex_descriptor_set_layout;
    }
  }

  {
    VkDescriptorSetLayout descriptor_set_layouts[2];
    VkPipelineLayoutCreateInfo pipeline_layout_info;

    descriptor_set_layouts[0] = vk->ubo_vertex_descriptor_set_layout;
    descriptor_set_layouts[1] = vk->image_fragment_descriptor_set_layout;

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = NULL;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount =
        owl_array_size(descriptor_set_layouts);
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    vk_result = vkCreatePipelineLayout(vk->device, &pipeline_layout_info, NULL,
                                       &vk->common_pipeline_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_fragment_descriptor_set_layout;
    }
  }

  {
    VkDescriptorSetLayout descriptor_set_layouts[6];
    VkPushConstantRange push_constant_range;
    VkPipelineLayoutCreateInfo pipeline_layout_info;

    push_constant_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(struct owl_model_material_push_constant);

    descriptor_set_layouts[0] = vk->ubo_both_descriptor_set_layout;
    descriptor_set_layouts[1] = vk->image_fragment_descriptor_set_layout;
    descriptor_set_layouts[2] = vk->image_fragment_descriptor_set_layout;
    descriptor_set_layouts[3] = vk->image_fragment_descriptor_set_layout;
    descriptor_set_layouts[4] = vk->ssbo_vertex_descriptor_set_layout;
    descriptor_set_layouts[5] = vk->ubo_fragment_descriptor_set_layout;

    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = NULL;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount =
        owl_array_size(descriptor_set_layouts);
    pipeline_layout_info.pSetLayouts = descriptor_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    vk_result = vkCreatePipelineLayout(vk->device, &pipeline_layout_info, NULL,
                                       &vk->model_pipeline_layout);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_common_pipeline_layout;
    }
  }
  goto out;

error_destroy_common_pipeline_layout:
  vkDestroyPipelineLayout(vk->device, vk->common_pipeline_layout, NULL);

error_destroy_image_fragment_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->image_fragment_descriptor_set_layout, NULL);

error_destroy_ssbo_vertex_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ssbo_vertex_descriptor_set_layout, NULL);

error_destroy_ubo_both_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(vk->device, vk->ubo_both_descriptor_set_layout,
                               NULL);

error_destroy_ubo_fragment_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ubo_fragment_descriptor_set_layout, NULL);

error_destroy_ubo_vertex_descriptor_set_layout:
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ubo_vertex_descriptor_set_layout, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_layouts(struct owl_vk_renderer *vk)
{
  vkDestroyPipelineLayout(vk->device, vk->model_pipeline_layout, NULL);
  vkDestroyPipelineLayout(vk->device, vk->common_pipeline_layout, NULL);
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->image_fragment_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ssbo_vertex_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(vk->device, vk->ubo_both_descriptor_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ubo_fragment_descriptor_set_layout, NULL);
  vkDestroyDescriptorSetLayout(vk->device,
                               vk->ubo_vertex_descriptor_set_layout, NULL);
}

owl_public enum owl_code
owl_vk_init_pipelines(struct owl_vk_renderer *vk, owl_u32 w, owl_u32 h)
{
  VkResult vk_result;
  enum owl_code code = OWL_OK;

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
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = w;
    scissor.extent.height = h;

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
    multisample.rasterizationSamples = vk->msaa;
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
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vk->basic_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk->basic_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    vk->pipeline_layouts[OWL_VK_PIPELINE_BASIC] = vk->common_pipeline_layout;

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
    pipeline_info.layout = vk->pipeline_layouts[OWL_VK_PIPELINE_BASIC];
    pipeline_info.renderPass = vk->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    vk_result = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &vk->pipelines[OWL_VK_PIPELINE_BASIC]);
  }

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
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = w;
    scissor.extent.height = h;

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
    rast.polygonMode = VK_POLYGON_MODE_LINE;
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
    multisample.rasterizationSamples = vk->msaa;
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
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vk->basic_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk->basic_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    vk->pipeline_layouts[OWL_VK_PIPELINE_WIRES] = vk->common_pipeline_layout;

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
    pipeline_info.layout = vk->pipeline_layouts[OWL_VK_PIPELINE_WIRES];
    pipeline_info.renderPass = vk->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &vk->pipelines[OWL_VK_PIPELINE_WIRES]);
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
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = w;
    scissor.extent.height = h;

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
    multisample.rasterizationSamples = vk->msaa;
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
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vk->basic_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk->text_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    vk->pipeline_layouts[OWL_VK_PIPELINE_TEXT] = vk->common_pipeline_layout;

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
    pipeline_info.layout = vk->pipeline_layouts[OWL_VK_PIPELINE_TEXT];
    pipeline_info.renderPass = vk->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &vk->pipelines[OWL_VK_PIPELINE_TEXT]);
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
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = w;
    scissor.extent.height = h;

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
    multisample.rasterizationSamples = vk->msaa;
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
    owl_memset(&depth.front, 0, sizeof(depth.front));
    owl_memset(&depth.back, 0, sizeof(depth.back));
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vk->model_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk->model_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    vk->pipeline_layouts[OWL_VK_PIPELINE_MODEL] = vk->model_pipeline_layout;

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
    pipeline_info.layout = vk->pipeline_layouts[OWL_VK_PIPELINE_MODEL];
    pipeline_info.renderPass = vk->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &vk->pipelines[OWL_VK_PIPELINE_MODEL]);
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
    viewport.width = w;
    viewport.height = h;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = w;
    scissor.extent.height = h;

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
    multisample.rasterizationSamples = vk->msaa;
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
    stages[0].module = vk->skybox_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk->skybox_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    vk->pipeline_layouts[OWL_VK_PIPELINE_SKYBOX] = vk->common_pipeline_layout;

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
    pipeline_info.layout = vk->pipeline_layouts[OWL_VK_PIPELINE_SKYBOX];
    pipeline_info.renderPass = vk->main_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    /* TODO(samuel): create all pipelines in a single batch */
    vk_result = vkCreateGraphicsPipelines(
        vk->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL,
        &vk->pipelines[OWL_VK_PIPELINE_SKYBOX]);

    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_model_pipeline;
    }
  }

  goto out;

error_destroy_model_pipeline:
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_MODEL], NULL);

error_destroy_text_pipeline:
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_TEXT], NULL);

error_destroy_wires_pipeline:
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_TEXT], NULL);

error_destroy_basic_pipeline:
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_BASIC], NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_pipelines(struct owl_vk_renderer *vk)
{
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_SKYBOX], NULL);
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_MODEL], NULL);
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_TEXT], NULL);
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_WIRES], NULL);
  vkDestroyPipeline(vk->device, vk->pipelines[OWL_VK_PIPELINE_BASIC], NULL);
}

owl_public enum owl_code
owl_vk_init_upload_heap(struct owl_vk_renderer *vk, owl_u64 size)
{
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements memory_requirements;
  VkMemoryAllocateInfo memory_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  vk->upload_heap_in_use = 0;
  vk->upload_heap_data = NULL;
  vk->upload_heap_size = size;

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = size;
  buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = NULL;

  vk_result = vkCreateBuffer(vk->device, &buffer_info, NULL,
                             &vk->upload_heap_buffer);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto out;
  }

  vkGetBufferMemoryRequirements(vk->device, vk->upload_heap_buffer,
                                &memory_requirements);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = memory_requirements.size;
  memory_info.memoryTypeIndex =
      owl_vk_find_memory_type(vk, memory_requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vk_result = vkAllocateMemory(vk->device, &memory_info, NULL,
                               &vk->upload_heap_memory);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_destroy_buffer;
  }

  vk_result = vkBindBufferMemory(vk->device, vk->upload_heap_buffer,
                                 vk->upload_heap_memory, 0);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  vk_result = vkMapMemory(vk->device, vk->upload_heap_memory, 0,
                          vk->upload_heap_size, 0, &vk->upload_heap_data);
  if (vk_result) {
    code = OWL_ERROR_FATAL;
    goto error_free_memory;
  }

  goto out;

error_free_memory:
  vkFreeMemory(vk->device, vk->upload_heap_memory, NULL);

error_destroy_buffer:
  vkDestroyBuffer(vk->device, vk->upload_heap_buffer, NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_upload_heap(struct owl_vk_renderer *vk)
{
  vkFreeMemory(vk->device, vk->upload_heap_memory, NULL);
  vkDestroyBuffer(vk->device, vk->upload_heap_buffer, NULL);
}

owl_public enum owl_code
owl_vk_init_frame_heaps(struct owl_vk_renderer *vk, owl_u64 size)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  vk->frame_heap_size = size;
  vk->frame_heap_offset = 0;
  vk->frame_heap_alignment = 0;

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
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

    vk_result = vkCreateBuffer(vk->device, &buffer_info, NULL,
                               &vk->frame_heap_buffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_buffers;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkMemoryRequirements memory_requirements;
    VkMemoryAllocateInfo memory_info;

    vkGetBufferMemoryRequirements(vk->device, vk->frame_heap_buffers[0],
                                  &memory_requirements);

    vk->frame_heap_alignment = memory_requirements.alignment;

    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.pNext = NULL;
    memory_info.allocationSize = memory_requirements.size;
    memory_info.memoryTypeIndex =
        owl_vk_find_memory_type(vk, memory_requirements.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    vk_result = vkAllocateMemory(vk->device, &memory_info, NULL,
                                 &vk->frame_heap_memories[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }

    vk_result = vkBindBufferMemory(vk->device, vk->frame_heap_buffers[i],
                                   vk->frame_heap_memories[i], 0);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }

    vk_result = vkMapMemory(vk->device, vk->frame_heap_memories[i], 0,
                            vk->frame_heap_size, 0, &vk->frame_heap_data[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_memories;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkDescriptorSetAllocateInfo descriptor_set_info;

    descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_info.pNext = NULL;
    descriptor_set_info.descriptorPool = vk->descriptor_pool;
    descriptor_set_info.descriptorSetCount = 1;
    descriptor_set_info.pSetLayouts = &vk->ubo_vertex_descriptor_set_layout;

    vk_result =
        vkAllocateDescriptorSets(vk->device, &descriptor_set_info,
                                 &vk->frame_heap_pvm_descriptor_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_pvm_descriptor_sets;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkDescriptorSetAllocateInfo descriptor_set_info;

    descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_info.pNext = NULL;
    descriptor_set_info.descriptorPool = vk->descriptor_pool;
    descriptor_set_info.descriptorSetCount = 1;
    descriptor_set_info.pSetLayouts = &vk->ubo_both_descriptor_set_layout;

    vk_result =
        vkAllocateDescriptorSets(vk->device, &descriptor_set_info,
                                 &vk->frame_heap_model1_descriptor_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_model1_descriptor_sets;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkDescriptorSetAllocateInfo descriptor_set_info;

    descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_info.pNext = NULL;
    descriptor_set_info.descriptorPool = vk->descriptor_pool;
    descriptor_set_info.descriptorSetCount = 1;
    descriptor_set_info.pSetLayouts = &vk->ubo_fragment_descriptor_set_layout;

    vk_result =
        vkAllocateDescriptorSets(vk->device, &descriptor_set_info,
                                 &vk->frame_heap_model2_descriptor_sets[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_free_model2_descriptor_sets;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {

    VkDescriptorBufferInfo descriptors[3];
    VkWriteDescriptorSet writes[3];

    descriptors[0].buffer = vk->frame_heap_buffers[i];
    descriptors[0].offset = 0;
    descriptors[0].range = sizeof(struct owl_pvm_ubo);

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = vk->frame_heap_pvm_descriptor_sets[i];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pImageInfo = NULL;
    writes[0].pBufferInfo = &descriptors[0];
    writes[0].pTexelBufferView = NULL;

    descriptors[1].buffer = vk->frame_heap_buffers[i];
    descriptors[1].offset = 0;
    descriptors[1].range = sizeof(struct owl_model_ubo1);

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = vk->frame_heap_model1_descriptor_sets[i];
    writes[1].dstBinding = 0;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[1].pImageInfo = NULL;
    writes[1].pBufferInfo = &descriptors[1];
    writes[1].pTexelBufferView = NULL;

    descriptors[2].buffer = vk->frame_heap_buffers[i];
    descriptors[2].offset = 0;
    descriptors[2].range = sizeof(struct owl_model_ubo2);

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].pNext = NULL;
    writes[2].dstSet = vk->frame_heap_model2_descriptor_sets[i];
    writes[2].dstBinding = 0;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[2].pImageInfo = NULL;
    writes[2].pBufferInfo = &descriptors[2];
    writes[2].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(vk->device, owl_array_size(writes), writes, 0,
                           NULL);
  }

  goto out;

error_free_model2_descriptor_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_model2_descriptor_sets[i]);

  i = vk->num_swapchain_images;

error_free_model1_descriptor_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_model1_descriptor_sets[i]);

  i = vk->num_swapchain_images;

error_free_pvm_descriptor_sets:
  for (i = i - 1; i >= 0; --i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_pvm_descriptor_sets[i]);

  i = vk->num_swapchain_images;

error_free_memories:
  for (i = i - 1; i >= 0; --i)
    vkFreeMemory(vk->device, vk->frame_heap_memories[i], NULL);

  i = vk->num_swapchain_images;

error_destroy_buffers:
  for (i = i - 1; i >= 0; --i)
    vkDestroyBuffer(vk->device, vk->frame_heap_buffers[i], NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_frame_heaps(struct owl_vk_renderer *vk)
{
  owl_u32 i;

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_model2_descriptor_sets[i]);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_model1_descriptor_sets[i]);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                         &vk->frame_heap_pvm_descriptor_sets[i]);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkFreeMemory(vk->device, vk->frame_heap_memories[i], NULL);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroyBuffer(vk->device, vk->frame_heap_buffers[i], NULL);
}

owl_public enum owl_code
owl_vk_init_frames(struct owl_vk_renderer *vk, owl_u64 size)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_OK;

  vk->frame = 0;

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i)
    vk->num_frame_garbage_buffers[i] = 0;

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i)
    vk->num_frame_garbage_memories[i] = 0;

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i)
    vk->num_frame_garbage_descriptor_sets[i] = 0;

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkCommandPoolCreateInfo command_pool_info;

    command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_info.pNext = NULL;
    command_pool_info.flags = 0;
    command_pool_info.queueFamilyIndex = vk->graphics_queue_family;

    vk_result = vkCreateCommandPool(vk->device, &command_pool_info, NULL,
                                    &vk->frame_command_pools[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_command_pools;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkCommandBufferAllocateInfo command_buffer_info;

    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.pNext = NULL;
    command_buffer_info.commandPool = vk->frame_command_pools[i];
    command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers(vk->device, &command_buffer_info,
                                         &vk->frame_command_buffers[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_command_buffers;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkFenceCreateInfo fence_info;

    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = NULL;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence(vk->device, &fence_info, NULL,
                              &vk->frame_in_flight_fences[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_in_flight_fences;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkSemaphoreCreateInfo semaphore_info;

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;

    vk_result = vkCreateSemaphore(vk->device, &semaphore_info, NULL,
                                  &vk->frame_image_available_semaphores[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_image_available_semaphores;
    }
  }

  for (i = 0; i < (owl_i32)vk->num_swapchain_images; ++i) {
    VkSemaphoreCreateInfo semaphore_info;

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;

    vk_result = vkCreateSemaphore(vk->device, &semaphore_info, NULL,
                                  &vk->frame_render_done_semaphores[i]);
    if (vk_result) {
      code = OWL_ERROR_FATAL;
      goto error_destroy_render_done_semaphores;
    }
  }

  code = owl_vk_init_frame_heaps(vk, size);
  if (code)
    goto error_destroy_render_done_semaphores;

  goto out;

error_destroy_render_done_semaphores:
  for (i = i - 1; i >= 0; --i)
    vkDestroySemaphore(vk->device, vk->frame_render_done_semaphores[i], NULL);

error_destroy_image_available_semaphores:
  for (i = i - 1; i >= 0; --i)
    vkDestroySemaphore(vk->device, vk->frame_image_available_semaphores[i],
                       NULL);

error_destroy_in_flight_fences:
  for (i = i - 1; i >= 0; --i)
    vkDestroyFence(vk->device, vk->frame_in_flight_fences[i], NULL);

  i = vk->num_swapchain_images;

error_destroy_command_buffers:
  for (i = i - 1; i >= 0; --i)
    vkFreeCommandBuffers(vk->device, vk->frame_command_pools[i], 1,
                         &vk->frame_command_buffers[i]);

  i = vk->num_swapchain_images;

error_destroy_command_pools:
  for (i = i - 1; i >= 0; --i)
    vkDestroyCommandPool(vk->device, vk->frame_command_pools[i], NULL);

out:
  return code;
}

owl_public void
owl_vk_deinit_frames(struct owl_vk_renderer *vk)
{
  owl_u32 i;

  for (i = 0; i < vk->num_swapchain_images; ++i) {
    owl_u32 j;
    for (j = 0; j < vk->num_frame_garbage_descriptor_sets[i]; ++j)
      vkFreeDescriptorSets(vk->device, vk->descriptor_pool, 1,
                           &vk->frame_garbage_descriptor_sets[i][j]);
  }

  for (i = 0; i < vk->num_swapchain_images; ++i) {
    owl_u32 j;
    for (j = 0; j < vk->num_frame_garbage_descriptor_sets[i]; ++j)
      vkFreeMemory(vk->device, vk->frame_garbage_memories[i][j], NULL);
  }

  owl_vk_deinit_frame_heaps(vk);

  for (i = 0; i < vk->num_swapchain_images; ++i) {
    owl_u32 j;
    for (j = 0; j < vk->num_frame_garbage_descriptor_sets[i]; ++j)
      vkDestroyBuffer(vk->device, vk->frame_garbage_buffers[i][j], NULL);
  }

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroySemaphore(vk->device, vk->frame_render_done_semaphores[i], NULL);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroySemaphore(vk->device, vk->frame_image_available_semaphores[i],
                       NULL);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroyFence(vk->device, vk->frame_in_flight_fences[i], NULL);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkFreeCommandBuffers(vk->device, vk->frame_command_pools[i], 1,
                         &vk->frame_command_buffers[i]);

  for (i = 0; i < vk->num_swapchain_images; ++i)
    vkDestroyCommandPool(vk->device, vk->frame_command_pools[i], NULL);
}

owl_public enum owl_code
owl_vk_init_samplers(struct owl_vk_renderer *vk)
{
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

  vk_result = vkCreateSampler(vk->device, &sampler_info, NULL,
                              &vk->linear_sampler);
  if (vk_result)
    return OWL_ERROR_FATAL;

  return OWL_OK;
}

owl_public void
owl_vk_deinit_samplers(struct owl_vk_renderer *vk)
{
  vkDestroySampler(vk->device, vk->linear_sampler, NULL);
}

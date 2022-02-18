#include "renderer.h"

#include "internal.h"
#include "model.h"
#include "skybox.h"
#include "window.h"

OWL_GLOBAL char const *const g_required_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef OWL_ENABLE_VALIDATION

OWL_GLOBAL char const *const g_debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif /* OWL_ENABLE_VALIDATION */

OWL_INTERNAL enum owl_code
owl_renderer_init_instance_(struct owl_renderer_info const *info,
                            struct owl_renderer *r) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance;
  enum owl_code code = OWL_SUCCESS;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = info->name;
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pNext = NULL;
  instance.flags = 0;
  instance.pApplicationInfo = &app;
#ifdef OWL_ENABLE_VALIDATION
  instance.enabledLayerCount = OWL_ARRAY_SIZE(g_debug_validation_layers);
  instance.ppEnabledLayerNames = g_debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
  instance.enabledLayerCount = 0;
  instance.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
  instance.enabledExtensionCount = (owl_u32)info->instance_extension_count;
  instance.ppEnabledExtensionNames = info->instance_extensions;

  OWL_CHECK(vkCreateInstance(&instance, NULL, &r->instance));

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_instance_(struct owl_renderer *r) {
  vkDestroyInstance(r->instance, NULL);
}

#ifdef OWL_ENABLE_VALIDATION

#define OWL_GET_INSTANCE_PROC_ADDR(i, fn)                                      \
  ((PFN_##fn)vkGetInstanceProcAddr((i), #fn))

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32 owl_vk_debug_callback_(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user_data) {
  OWL_UNUSED(type);
  OWL_UNUSED(user_data);

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    fprintf(stderr, "%s\n", data->pMessage);
  else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    fprintf(stderr, "%s\n", data->pMessage);

  return VK_FALSE;
}

OWL_INTERNAL enum owl_code owl_renderer_init_debug_(struct owl_renderer *r) {
  VkDebugUtilsMessengerCreateInfoEXT debug;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
  enum owl_code code = OWL_SUCCESS;

  vkCreateDebugUtilsMessengerEXT =
      OWL_GET_INSTANCE_PROC_ADDR(r->instance, vkCreateDebugUtilsMessengerEXT);

  OWL_ASSERT(vkCreateDebugUtilsMessengerEXT);

#define OWL_DEBUG_SEVERITY_FLAGS                                               \
  (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |                           \
   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |                           \
   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)

#define OWL_DEBUG_TYPE_FLAGS                                                   \
  (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |                               \
   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |                            \
   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)

  debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug.pNext = NULL;
  debug.flags = 0;
  debug.messageSeverity = OWL_DEBUG_SEVERITY_FLAGS;
  debug.messageType = OWL_DEBUG_TYPE_FLAGS;
  debug.pfnUserCallback = owl_vk_debug_callback_;
  debug.pUserData = r;

#undef OWL_DEBUG_SEVERITY_FLAGS
#undef OWL_DEBUG_TYPE_FLAGS

  OWL_CHECK(
      vkCreateDebugUtilsMessengerEXT(r->instance, &debug, NULL, &r->debug));

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_debug_(struct owl_renderer *r) {
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger;

  vkDestroyDebugUtilsMessenger =
      OWL_GET_INSTANCE_PROC_ADDR(r->instance, vkDestroyDebugUtilsMessengerEXT);

  OWL_ASSERT(vkDestroyDebugUtilsMessenger);

  vkDestroyDebugUtilsMessenger(r->instance, r->debug, NULL);
}

#endif /* OWL_ENABLE_VALIDATION */

OWL_INTERNAL enum owl_code
owl_renderer_init_surface_(struct owl_renderer_info const *info,
                           struct owl_renderer *r) {
  return info->create_surface(r, info->surface_user_data, &r->surface);
}

OWL_INTERNAL void owl_renderer_deinit_surface_(struct owl_renderer *r) {
  vkDestroySurfaceKHR(r->instance, r->surface, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_fill_device_options_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  OWL_CHECK(
      vkEnumeratePhysicalDevices(r->instance, &r->device_options_count, NULL));

  if (OWL_RENDERER_MAX_DEVICE_OPTIONS <= r->device_options_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_CHECK(vkEnumeratePhysicalDevices(r->instance, &r->device_options_count,
                                       r->device_options));

end:
  return code;
}

OWL_INTERNAL int owl_renderer_query_families_(struct owl_renderer const *r,
                                              VkPhysicalDevice device,
                                              owl_u32 *graphics_family_index,
                                              owl_u32 *present_family_index) {
#define OWL_QUEUE_UNSELECTED (owl_u32) - 1

  int found;
  owl_u32 i, count;
  VkQueueFamilyProperties *properties;

  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);

  if (!(properties = OWL_MALLOC(count * sizeof(*properties))))
    return 0;

  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties);

  *graphics_family_index = OWL_QUEUE_UNSELECTED;
  *present_family_index = OWL_QUEUE_UNSELECTED;

  for (i = 0; i < count; ++i) {
    VkBool32 has_surface;

    if (!properties[i].queueCount)
      continue;

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags)
      *graphics_family_index = i;

    OWL_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, r->surface,
                                                   &has_surface));

    if (has_surface)
      *present_family_index = i;

    if (OWL_QUEUE_UNSELECTED == *graphics_family_index)
      continue;

    if (OWL_QUEUE_UNSELECTED == *present_family_index)
      continue;

    found = 1;
    goto end_free_properties;
  }

  found = 0;

end_free_properties:
  OWL_FREE(properties);

  return found;
}

OWL_INTERNAL int
owl_validate_device_extensions_(owl_u32 count,
                                VkExtensionProperties const *extensions) {
  owl_u32 i;
  owl_u32 j;
  int found[OWL_ARRAY_SIZE(g_required_device_extensions)];

  OWL_MEMSET(found, 0, sizeof(found));

  for (i = 0; i < count; ++i)
    for (j = 0; j < OWL_ARRAY_SIZE(g_required_device_extensions); ++j)
      if (!strcmp(g_required_device_extensions[j], extensions[i].extensionName))
        found[j] = 1;

  for (i = 0; i < OWL_ARRAY_SIZE(g_required_device_extensions); ++i)
    if (!found[i])
      return 0;

  return 1;
}

OWL_INTERNAL enum owl_code
owl_renderer_select_physical_device_(struct owl_renderer *r) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < r->device_options_count; ++i) {
    owl_u32 has_formats;
    owl_u32 has_modes;
    owl_u32 extension_count;
    VkExtensionProperties *extensions;

    OWL_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        r->device_options[i], r->surface, &has_formats, NULL));

    if (!has_formats)
      continue;

    OWL_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        r->device_options[i], r->surface, &has_modes, NULL));

    if (!has_modes)
      continue;

    if (!owl_renderer_query_families_(r, r->device_options[i],
                                      &r->graphics_family_index,
                                      &r->present_family_index))
      continue;

    vkGetPhysicalDeviceFeatures(r->device_options[i], &r->device_features);

    if (!r->device_features.samplerAnisotropy)
      continue;

    OWL_CHECK(vkEnumerateDeviceExtensionProperties(r->device_options[i], NULL,
                                                   &extension_count, NULL));

    if (!(extensions = OWL_MALLOC(extension_count * sizeof(*extensions)))) {
      code = OWL_ERROR_BAD_ALLOC;
      goto end;
    }

    OWL_CHECK(vkEnumerateDeviceExtensionProperties(
        r->device_options[i], NULL, &extension_count, extensions));

    if (!owl_validate_device_extensions_(extension_count, extensions)) {
      OWL_FREE(extensions);
      continue;
    }

    OWL_FREE(extensions);

    r->physical_device = r->device_options[i];

    vkGetPhysicalDeviceProperties(r->physical_device, &r->device_properties);

    vkGetPhysicalDeviceMemoryProperties(r->physical_device,
                                        &r->device_memory_properties);
    code = OWL_SUCCESS;
    goto end;
  }

  code = OWL_ERROR_NO_SUITABLE_DEVICE;

end:
  return code;
}

OWL_INTERNAL enum owl_code
owl_renderer_select_surface_format_(struct owl_renderer *r, VkFormat format,
                                    VkColorSpaceKHR color_space) {
  owl_u32 i;
  owl_u32 formats_count;
  VkSurfaceFormatKHR *formats;
  enum owl_code code = OWL_SUCCESS;

  OWL_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface,
                                                 &formats_count, NULL));

  if (!(formats = OWL_MALLOC(formats_count * sizeof(*formats)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  OWL_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface,
                                                 &formats_count, formats));

  for (i = 0; i < formats_count; ++i) {
    if (format != formats[i].format)
      continue;

    if (color_space != formats[i].colorSpace)
      continue;

    r->surface_format.format = format;
    r->surface_format.colorSpace = color_space;

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
owl_renderer_select_sample_count_(struct owl_renderer *r) {
  VkSampleCountFlags const samples =
      r->device_properties.limits.framebufferColorSampleCounts &
      r->device_properties.limits.framebufferDepthSampleCounts;
  enum owl_code code = OWL_SUCCESS;

  if (VK_SAMPLE_COUNT_2_BIT & samples) {
    r->msaa_sample_count = VK_SAMPLE_COUNT_2_BIT;
  } else {
    OWL_ASSERT(0 && "disabling multisampling is not supported");
    r->msaa_sample_count = VK_SAMPLE_COUNT_1_BIT;
    code = OWL_ERROR_UNKNOWN;
    goto end;
  }

end:
  return code;
}

OWL_INTERNAL owl_u32 owl_get_queue_count_(struct owl_renderer const *r) {
  return r->graphics_family_index == r->present_family_index ? 1 : 2;
}

OWL_INTERNAL enum owl_code owl_renderer_init_device_(struct owl_renderer *r) {
  VkDeviceCreateInfo device;
  VkDeviceQueueCreateInfo queues[2];
  float const priority = 1.0F;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_renderer_fill_device_options_(r)))
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_select_physical_device_(r)))
    goto end;

  code = owl_renderer_select_surface_format_(r, VK_FORMAT_B8G8R8A8_SRGB,
                                             VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);

  if (OWL_SUCCESS != code)
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_select_sample_count_(r)))
    goto end;

  queues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues[0].pNext = NULL;
  queues[0].flags = 0;
  queues[0].queueFamilyIndex = r->graphics_family_index;
  queues[0].queueCount = 1;
  queues[0].pQueuePriorities = &priority;

  queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues[1].pNext = NULL;
  queues[1].flags = 0;
  queues[1].queueFamilyIndex = r->present_family_index;
  queues[1].queueCount = 1;
  queues[1].pQueuePriorities = &priority;

  device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device.pNext = NULL;
  device.flags = 0;
  device.queueCreateInfoCount = owl_get_queue_count_(r);
  device.pQueueCreateInfos = queues;
  device.enabledLayerCount = 0;      /* deprecated */
  device.ppEnabledLayerNames = NULL; /* deprecated */
  device.enabledExtensionCount = OWL_ARRAY_SIZE(g_required_device_extensions);
  device.ppEnabledExtensionNames = g_required_device_extensions;
  device.pEnabledFeatures = &r->device_features;

  OWL_CHECK(vkCreateDevice(r->physical_device, &device, NULL, &r->device));
  vkGetDeviceQueue(r->device, r->graphics_family_index, 0, &r->graphics_queue);
  vkGetDeviceQueue(r->device, r->present_family_index, 0, &r->present_queue);

end:
  return code;
}

OWL_INTERNAL void owl_renderer_deinit_device_(struct owl_renderer *r) {
  vkDestroyDevice(r->device, NULL);
}

OWL_INTERNAL void owl_renderer_clamp_swapchain_extent_(owl_u32 width,
                                                       owl_u32 height,
                                                       struct owl_renderer *r) {
#define OWL_NO_RESTRICTIONS (owl_u32) - 1

  VkSurfaceCapabilitiesKHR capabilities;

  OWL_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      r->physical_device, r->surface, &capabilities));

  if (OWL_NO_RESTRICTIONS == capabilities.currentExtent.width) {
    r->swapchain_extent = capabilities.currentExtent;
  } else {
    owl_u32 const min_width = capabilities.minImageExtent.width;
    owl_u32 const min_height = capabilities.minImageExtent.height;
    owl_u32 const max_width = capabilities.maxImageExtent.width;
    owl_u32 const max_height = capabilities.maxImageExtent.height;

    r->swapchain_extent.width = OWL_CLAMP(width, min_width, max_width);
    r->swapchain_extent.height = OWL_CLAMP(height, min_height, max_height);
  }

#undef OWL_NO_RESTRICTIONS
}

OWL_INTERNAL enum owl_code
owl_renderer_select_present_mode_(struct owl_renderer *r,
                                  VkPresentModeKHR mode) {
#define OWL_MAX_PRESENT_MODES 16

  owl_u32 i, count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];
  enum owl_code code = OWL_SUCCESS;

  OWL_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      r->physical_device, r->surface, &count, NULL));

  if (OWL_MAX_PRESENT_MODES <= count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      r->physical_device, r->surface, &count, modes));

  for (i = 0; i < count; ++i)
    if (mode == (r->swapchain_present_mode = modes[count - i - 1]))
      break;

end:
  return code;

#undef OWL_MAX_PRESENT_MODES
}

OWL_INTERNAL enum owl_code
owl_renderer_init_swapchain_(struct owl_renderer_info const *info,
                             struct owl_renderer *r) {
  owl_u32 i;
  owl_u32 families[2];
  VkSurfaceCapabilitiesKHR capabilities;
  VkSwapchainCreateInfoKHR swapchain;
  enum owl_code code = OWL_SUCCESS;

  families[0] = r->graphics_family_index;
  families[1] = r->present_family_index;

  owl_renderer_clamp_swapchain_extent_((owl_u32)info->framebuffer_width,
                                       (owl_u32)info->framebuffer_height, r);

  code = owl_renderer_select_present_mode_(r, VK_PRESENT_MODE_FIFO_KHR);

  if (OWL_SUCCESS != code)
    goto end;

  OWL_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      r->physical_device, r->surface, &capabilities));

  swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain.pNext = NULL;
  swapchain.flags = 0;
  swapchain.surface = r->surface;
  swapchain.minImageCount = 2;
  swapchain.imageFormat = r->surface_format.format;
  swapchain.imageColorSpace = r->surface_format.colorSpace;
  swapchain.imageExtent.width = r->swapchain_extent.width;
  swapchain.imageExtent.height = r->swapchain_extent.height;
  swapchain.imageArrayLayers = 1;
  swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain.preTransform = capabilities.currentTransform;
  swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain.presentMode = r->swapchain_present_mode;
  swapchain.clipped = VK_TRUE;
  swapchain.oldSwapchain = VK_NULL_HANDLE;

  if (r->graphics_family_index == r->present_family_index) {
    swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain.queueFamilyIndexCount = 0;
    swapchain.pQueueFamilyIndices = NULL;
  } else {
    swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain.queueFamilyIndexCount = owl_get_queue_count_(r);
    swapchain.pQueueFamilyIndices = families;
  }

  OWL_CHECK(vkCreateSwapchainKHR(r->device, &swapchain, NULL, &r->swapchain));

  OWL_CHECK(vkGetSwapchainImagesKHR(r->device, r->swapchain,
                                    &r->swapchain_images_count, NULL));

  if (OWL_RENDERER_MAX_SWAPCHAIN_IMAGES <= r->swapchain_images_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto end;
  }

  OWL_CHECK(vkGetSwapchainImagesKHR(r->device, r->swapchain,
                                    &r->swapchain_images_count,
                                    r->swapchain_images));

  for (i = 0; i < r->swapchain_images_count; ++i) {
    VkImageViewCreateInfo view;

    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = r->swapchain_images[i];
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = r->surface_format.format;
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_CHECK(
        vkCreateImageView(r->device, &view, NULL, &r->swapchain_views[i]));
  }

  r->swapchain_clear_values[0].color.float32[0] = 0.0F;
  r->swapchain_clear_values[0].color.float32[1] = 0.0F;
  r->swapchain_clear_values[0].color.float32[2] = 0.0F;
  r->swapchain_clear_values[0].color.float32[3] = 1.0F;
  r->swapchain_clear_values[1].depthStencil.depth = 1.0F;
  r->swapchain_clear_values[1].depthStencil.stencil = 0.0F;

end:
  return code;
}

OWL_INTERNAL void owl_renderer_deinit_swapchain_(struct owl_renderer *r) {
  owl_u32 i;

  for (i = 0; i < r->swapchain_images_count; ++i)
    vkDestroyImageView(r->device, r->swapchain_views[i], NULL);

  vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_common_pools_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkCommandPoolCreateInfo pool;

    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.pNext = NULL;
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = r->graphics_family_index;

    OWL_CHECK(
        vkCreateCommandPool(r->device, &pool, NULL, &r->transient_cmd_pool));
  }

  {
#define OWL_MAX_UNIFORM_SETS 16
#define OWL_MAX_SAMPLER_SETS 16
#define OWL_MAX_TEXTURE_SETS 16
#define OWL_MAX_SETS 48
    VkDescriptorPoolSize sizes[3];
    VkDescriptorPoolCreateInfo pool;

    sizes[0].descriptorCount = OWL_MAX_UNIFORM_SETS;
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

    sizes[1].descriptorCount = OWL_MAX_SAMPLER_SETS;
    sizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;

    sizes[2].descriptorCount = OWL_MAX_TEXTURE_SETS;
    sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.pNext = NULL;
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = OWL_MAX_SETS;
    pool.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    pool.pPoolSizes = sizes;

    OWL_CHECK(
        vkCreateDescriptorPool(r->device, &pool, NULL, &r->common_set_pool));
#undef OWL_MAX_SETS
#undef OWL_MAX_SAMPLER_SETS
#undef OWL_MAX_UNIFORM_SETS
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_common_pools_(struct owl_renderer *r) {
  vkDestroyDescriptorPool(r->device, r->common_set_pool, NULL);
  vkDestroyCommandPool(r->device, r->transient_cmd_pool, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_main_render_pass_(struct owl_renderer *r) {
  VkAttachmentReference references[3];
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo render_pass;
  enum owl_code code = OWL_SUCCESS;

  references[0].attachment = 0;
  references[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  references[1].attachment = 1;
  references[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  references[2].attachment = 2;
  references[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
  attachments[1].format = VK_FORMAT_D32_SFLOAT;
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
  subpass.pColorAttachments = &references[0];
  subpass.pResolveAttachments = &references[2];
  subpass.pDepthStencilAttachment = &references[1];
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

#define OWL_SRC_STAGE                                                          \
  (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                             \
   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)

#define OWL_DST_STAGE                                                          \
  (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                             \
   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)

#define OWL_DST_ACCESS                                                         \
  (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |                                      \
   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = OWL_SRC_STAGE;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = OWL_DST_STAGE;
  dependency.dstAccessMask = OWL_DST_ACCESS;
  dependency.dependencyFlags = 0;

#undef OWL_DST_ACCESS
#undef OWL_DST_STAGE
#undef OWL_SRC_STAGE

  render_pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass.pNext = NULL;
  render_pass.flags = 0;
  render_pass.attachmentCount = OWL_ARRAY_SIZE(attachments);
  render_pass.pAttachments = attachments;
  render_pass.subpassCount = 1;
  render_pass.pSubpasses = &subpass;
  render_pass.dependencyCount = 1;
  render_pass.pDependencies = &dependency;

  OWL_CHECK(
      vkCreateRenderPass(r->device, &render_pass, NULL, &r->main_render_pass));

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_main_render_pass_(struct owl_renderer *r) {
  vkDestroyRenderPass(r->device, r->main_render_pass, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_attachments_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = r->surface_format.format;
    image.extent.width = r->swapchain_extent.width;
    image.extent.height = r->swapchain_extent.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = r->msaa_sample_count;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_CHECK(vkCreateImage(r->device, &image, NULL, &r->color_image));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetImageMemoryRequirements(r->device, r->color_image, &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_CHECK(vkAllocateMemory(r->device, &memory, NULL, &r->color_memory));

    OWL_CHECK(vkBindImageMemory(r->device, r->color_image, r->color_memory, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = r->color_image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = r->surface_format.format;
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_CHECK(vkCreateImageView(r->device, &view, NULL, &r->color_view));
  }

  {
    VkImageCreateInfo image;
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.flags = 0;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = VK_FORMAT_D32_SFLOAT;
    image.extent.width = r->swapchain_extent.width;
    image.extent.height = r->swapchain_extent.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = r->msaa_sample_count;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image.queueFamilyIndexCount = 0;
    image.pQueueFamilyIndices = NULL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_CHECK(vkCreateImage(r->device, &image, NULL, &r->depth_stencil_image));
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;
    vkGetImageMemoryRequirements(r->device, r->depth_stencil_image,
                                 &requirements);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize = requirements.size;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_GPU_ONLY);

    OWL_CHECK(
        vkAllocateMemory(r->device, &memory, NULL, &r->depth_stencil_memory));

    OWL_CHECK(vkBindImageMemory(r->device, r->depth_stencil_image,
                                r->depth_stencil_memory, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = r->depth_stencil_image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_D32_SFLOAT;
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_CHECK(
        vkCreateImageView(r->device, &view, NULL, &r->depth_stencil_view));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_attachments_(struct owl_renderer *r) {
  vkDestroyImageView(r->device, r->depth_stencil_view, NULL);
  vkFreeMemory(r->device, r->depth_stencil_memory, NULL);
  vkDestroyImage(r->device, r->depth_stencil_image, NULL);

  vkDestroyImageView(r->device, r->color_view, NULL);
  vkFreeMemory(r->device, r->color_memory, NULL);
  vkDestroyImage(r->device, r->color_image, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_framebuffers_(struct owl_renderer *r) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < r->swapchain_images_count; ++i) {
    VkImageView attachments[3];
    VkFramebufferCreateInfo framebuffer;

    attachments[0] = r->color_view;
    attachments[1] = r->depth_stencil_view;
    attachments[2] = r->swapchain_views[i];

    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.pNext = NULL;
    framebuffer.flags = 0;
    framebuffer.renderPass = r->main_render_pass;
    framebuffer.attachmentCount = OWL_ARRAY_SIZE(attachments);
    framebuffer.pAttachments = attachments;
    framebuffer.width = r->swapchain_extent.width;
    framebuffer.height = r->swapchain_extent.height;
    framebuffer.layers = 1;

    OWL_CHECK(vkCreateFramebuffer(r->device, &framebuffer, NULL,
                                  &r->swapchain_framebuffers[i]));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_framebuffers_(struct owl_renderer *r) {
  owl_u32 i;

  for (i = 0; i < r->swapchain_images_count; ++i)
    vkDestroyFramebuffer(r->device, r->swapchain_framebuffers[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_set_layouts_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo layout;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = 1;
    layout.pBindings = &binding;

    OWL_CHECK(vkCreateDescriptorSetLayout(r->device, &layout, NULL,
                                          &r->pvm_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo layout;

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

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = OWL_ARRAY_SIZE(bindings);
    layout.pBindings = bindings;

    OWL_CHECK(vkCreateDescriptorSetLayout(r->device, &layout, NULL,
                                          &r->texture_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding bindings[5];
    VkDescriptorSetLayoutCreateInfo layout;

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = NULL;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = NULL;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = NULL;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = NULL;

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = OWL_ARRAY_SIZE(bindings);
    layout.pBindings = bindings;

    OWL_CHECK(vkCreateDescriptorSetLayout(r->device, &layout, NULL,
                                          &r->scene_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding bindings[5];
    VkDescriptorSetLayoutCreateInfo layout;

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = NULL;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = NULL;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = NULL;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = NULL;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = NULL;

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = OWL_ARRAY_SIZE(bindings);
    layout.pBindings = bindings;

    OWL_CHECK(vkCreateDescriptorSetLayout(r->device, &layout, NULL,
                                          &r->material_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo layout;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    binding.pImmutableSamplers = NULL;

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = 1;
    layout.pBindings = &binding;

    OWL_CHECK(vkCreateDescriptorSetLayout(r->device, &layout, NULL,
                                          &r->node_set_layout));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_set_layouts_(struct owl_renderer *r) {
  vkDestroyDescriptorSetLayout(r->device, r->node_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->material_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->scene_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->texture_set_layout, NULL);
  vkDestroyDescriptorSetLayout(r->device, r->pvm_set_layout, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_pipelines_layouts_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkDescriptorSetLayout sets[2];
    VkPipelineLayoutCreateInfo layout;

    sets[0] = r->pvm_set_layout;
    sets[1] = r->texture_set_layout;

    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.setLayoutCount = OWL_ARRAY_SIZE(sets);
    layout.pSetLayouts = sets;
    layout.pushConstantRangeCount = 0;
    layout.pPushConstantRanges = NULL;

    OWL_CHECK(vkCreatePipelineLayout(r->device, &layout, NULL,
                                     &r->common_pipeline_layout));
  }

  {
    VkPushConstantRange range;
    VkDescriptorSetLayout sets[3];
    VkPipelineLayoutCreateInfo layout;

    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = sizeof(struct owl_material_push_constant);

    sets[0] = r->scene_set_layout;
    sets[1] = r->material_set_layout;
    sets[2] = r->node_set_layout;

    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.setLayoutCount = OWL_ARRAY_SIZE(sets);
    layout.pSetLayouts = sets;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &range;

    OWL_CHECK(vkCreatePipelineLayout(r->device, &layout, NULL,
                                     &r->pbr_pipeline_layout));
  }

  return code;
}

OWL_INTERNAL void
owl_renderer_deinit_pipeline_layouts_(struct owl_renderer *r) {
  vkDestroyPipelineLayout(r->device, r->pbr_pipeline_layout, NULL);
  vkDestroyPipelineLayout(r->device, r->common_pipeline_layout, NULL);
}

OWL_INTERNAL enum owl_code owl_renderer_init_shaders_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo shader;

    /* TODO(samuel): Properly load code at runtime */
    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/basic_vertex.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->basic_vertex_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/basic_fragment.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->basic_fragment_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/font_fragment.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->font_fragment_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/skybox_vertex.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->skybox_vertex_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/skybox_fragment.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->skybox_fragment_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/pbr_vertex.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(
        vkCreateShaderModule(r->device, &shader, NULL, &r->pbr_vertex_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/pbr_fragment.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_CHECK(vkCreateShaderModule(r->device, &shader, NULL,
                                   &r->pbr_fragment_shader));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_shaders_(struct owl_renderer *r) {
  vkDestroyShaderModule(r->device, r->pbr_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->pbr_vertex_shader, NULL);
  vkDestroyShaderModule(r->device, r->skybox_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->skybox_vertex_shader, NULL);
  vkDestroyShaderModule(r->device, r->font_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->basic_fragment_shader, NULL);
  vkDestroyShaderModule(r->device, r->basic_vertex_shader, NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_pipelines_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  /* basic */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[3];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attrs[0].binding = 0;
    vertex_attrs[0].location = 0;
    vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[0].offset = offsetof(struct owl_draw_cmd_vertex, position);
    vertex_attrs[1].binding = 0;
    vertex_attrs[1].location = 1;
    vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vertex_attrs[2].binding = 0;
    vertex_attrs[2].location = 2;
    vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attrs[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

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

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->common_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &pipeline,
                                        NULL,
                                        &r->pipelines[OWL_PIPELINE_TYPE_MAIN]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_MAIN] = r->common_pipeline_layout;
  }

  /* wires */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[3];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attrs[0].binding = 0;
    vertex_attrs[0].location = 0;
    vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[0].offset = offsetof(struct owl_draw_cmd_vertex, position);
    vertex_attrs[1].binding = 0;
    vertex_attrs[1].location = 1;
    vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vertex_attrs[2].binding = 0;
    vertex_attrs[2].location = 2;
    vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attrs[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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
    rasterization.polygonMode = VK_POLYGON_MODE_LINE;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.depthBiasConstantFactor = 0.0F;
    rasterization.depthBiasClamp = 0.0F;
    rasterization.depthBiasSlopeFactor = 0.0F;
    rasterization.lineWidth = 1.0F;

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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

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

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->common_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(
        vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
                                  &r->pipelines[OWL_PIPELINE_TYPE_WIRES]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_WIRES] = r->common_pipeline_layout;
  }

  /* font */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[3];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attrs[0].binding = 0;
    vertex_attrs[0].location = 0;
    vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[0].offset = offsetof(struct owl_draw_cmd_vertex, position);
    vertex_attrs[1].binding = 0;
    vertex_attrs[1].location = 1;
    vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vertex_attrs[2].binding = 0;
    vertex_attrs[2].location = 2;
    vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attrs[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

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

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->common_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &pipeline,
                                        NULL,
                                        &r->pipelines[OWL_PIPELINE_TYPE_FONT]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_FONT] = r->common_pipeline_layout;
  }

  /* skybox */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[1];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_skybox_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attrs[0].binding = 0;
    vertex_attrs[0].location = 0;
    vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attrs[0].offset = offsetof(struct owl_skybox_vertex, position);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.depthCompareOp = VK_COMPARE_OP_LESS;
    depth.depthBoundsTestEnable = VK_FALSE;
    depth.stencilTestEnable = VK_FALSE;
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = r->skybox_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = r->skybox_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->common_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(
        vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
                                  &r->pipelines[OWL_PIPELINE_TYPE_SKYBOX]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_SKYBOX] = r->common_pipeline_layout;
  }

  /* pbr */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[6];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_model_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

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
    vertex_attrs[4].offset = offsetof(struct owl_model_vertex, joint0);
    vertex_attrs[5].binding = 0;
    vertex_attrs[5].location = 5;
    vertex_attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attrs[5].offset = offsetof(struct owl_model_vertex, weight0);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = r->pbr_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = r->pbr_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->pbr_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1, &pipeline,
                                        NULL,
                                        &r->pipelines[OWL_PIPELINE_TYPE_PBR]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_PBR] = r->pbr_pipeline_layout;
  }

  /* pbr alpha blend */
  {
    VkVertexInputBindingDescription vertex_binding;
    VkVertexInputAttributeDescription vertex_attrs[6];
    VkPipelineVertexInputStateCreateInfo input;
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
    VkGraphicsPipelineCreateInfo pipeline;

    vertex_binding.binding = 0;
    vertex_binding.stride = sizeof(struct owl_model_vertex);
    vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

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
    vertex_attrs[4].offset = offsetof(struct owl_model_vertex, joint0);
    vertex_attrs[5].binding = 0;
    vertex_attrs[5].location = 5;
    vertex_attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attrs[5].offset = offsetof(struct owl_model_vertex, weight0);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vertex_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vertex_attrs);
    input.pVertexAttributeDescriptions = vertex_attrs;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = r->swapchain_extent.width;
    viewport.height = r->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = r->swapchain_extent;

    viewport_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
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

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

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
    depth.minDepthBounds = 0.0F;
    depth.maxDepthBounds = 1.0F;

    OWL_MEMSET(&depth.front, 0, sizeof(depth.front));
    OWL_MEMSET(&depth.back, 0, sizeof(depth.back));

    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].pNext = NULL;
    stages[0].flags = 0;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = r->pbr_vertex_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = r->pbr_fragment_shader;
    stages[1].pName = "main";
    stages[1].pSpecializationInfo = NULL;

    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.pNext = NULL;
    pipeline.flags = 0;
    pipeline.stageCount = OWL_ARRAY_SIZE(stages);
    pipeline.pStages = stages;
    pipeline.pVertexInputState = &input;
    pipeline.pInputAssemblyState = &input_assembly;
    pipeline.pTessellationState = NULL;
    pipeline.pViewportState = &viewport_state;
    pipeline.pRasterizationState = &rasterization;
    pipeline.pMultisampleState = &multisample;
    pipeline.pDepthStencilState = &depth;
    pipeline.pColorBlendState = &color;
    pipeline.pDynamicState = NULL;
    pipeline.layout = r->pbr_pipeline_layout;
    pipeline.renderPass = r->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_CHECK(vkCreateGraphicsPipelines(
        r->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &r->pipelines[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND]));

    r->pipeline_layouts[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND] =
        r->pbr_pipeline_layout;
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_pipelines(struct owl_renderer *r) {
  /* NOTE: could loop it, but it's a good remainder */

  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND],
                    NULL);
  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_PBR], NULL);
  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_SKYBOX], NULL);
  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_FONT], NULL);
  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_WIRES], NULL);
  vkDestroyPipeline(r->device, r->pipelines[OWL_PIPELINE_TYPE_MAIN], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_frame_cmds_(struct owl_renderer *r) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
    VkCommandPoolCreateInfo cmd_pool;

    cmd_pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool.pNext = NULL;
    cmd_pool.flags = 0;
    cmd_pool.queueFamilyIndex = r->graphics_family_index;

    OWL_CHECK(vkCreateCommandPool(r->device, &cmd_pool, NULL,
                                  &r->frame_cmd_pools[i]));
  }

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
    VkCommandBufferAllocateInfo cmd;

    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = r->frame_cmd_pools[i];
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    OWL_CHECK(
        vkAllocateCommandBuffers(r->device, &cmd, &r->frame_cmd_buffers[i]));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_frame_cmds_(struct owl_renderer *r) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkFreeCommandBuffers(r->device, r->frame_cmd_pools[i], 1,
                         &r->frame_cmd_buffers[i]);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyCommandPool(r->device, r->frame_cmd_pools[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_frame_sync_(struct owl_renderer *r) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore;
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore.pNext = NULL;
    semaphore.flags = 0;
    OWL_CHECK(vkCreateSemaphore(r->device, &semaphore, NULL,
                                &r->image_available_semaphores[i]));
  }

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore;
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore.pNext = NULL;
    semaphore.flags = 0;
    OWL_CHECK(vkCreateSemaphore(r->device, &semaphore, NULL,
                                &r->render_done_semaphores[i]));
  }

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
    VkFenceCreateInfo fence;
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.pNext = NULL;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    OWL_CHECK(vkCreateFence(r->device, &fence, NULL, &r->in_flight_fences[i]));
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_frame_sync_(struct owl_renderer *r) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(r->device, r->image_available_semaphores[i], NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(r->device, r->render_done_semaphores[i], NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyFence(r->device, r->in_flight_fences[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_renderer_init_dynamic_buffer_(struct owl_renderer *r, VkDeviceSize size) {
  owl_u32 i;
  enum owl_code code = OWL_SUCCESS;

  r->active = 0;
  r->dynamic_buffer_size = size;

  /* init buffers */
  {
#define OWL_DOUBLE_BUFFER_USAGE                                                \
  (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |      \
   VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)

    VkBufferCreateInfo buffer;

    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.pNext = NULL;
    buffer.flags = 0;
    buffer.size = size;
    buffer.usage = OWL_DOUBLE_BUFFER_USAGE;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer.queueFamilyIndexCount = 0;
    buffer.pQueueFamilyIndices = NULL;

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_CHECK(
          vkCreateBuffer(r->device, &buffer, NULL, &r->dynamic_buffers[i]));

#undef OWL_DOUBLE_BUFFER_USAGE
  }

  /* init memory */
  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory;

    vkGetBufferMemoryRequirements(r->device, r->dynamic_buffers[0],
                                  &requirements);

    r->dynamic_buffer_alignment = requirements.alignment;
    r->dynamic_buffer_aligned_size = OWL_ALIGNU2(size, requirements.alignment);

    memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory.pNext = NULL;
    memory.allocationSize =
        r->dynamic_buffer_aligned_size * OWL_RENDERER_DYNAMIC_BUFFER_COUNT;
    memory.memoryTypeIndex = owl_renderer_find_memory_type(
        r, requirements.memoryTypeBits, OWL_MEMORY_VISIBILITY_CPU_ONLY);

    OWL_CHECK(vkAllocateMemory(r->device, &memory, NULL, &r->dynamic_memory));
  }

  /* bind buffers to memory */
  {
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_CHECK(vkBindBufferMemory(r->device, r->dynamic_buffers[i],
                                   r->dynamic_memory,
                                   i * r->dynamic_buffer_aligned_size));
  }

  /* map memory */
  {
    void *data;
    OWL_CHECK(
        vkMapMemory(r->device, r->dynamic_memory, 0, VK_WHOLE_SIZE, 0, &data));

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      r->dynamic_data[i] =
          (owl_byte *)data + i * r->dynamic_buffer_aligned_size;
  }

  /* init pvm descriptor sets */
  {
    VkDescriptorSetAllocateInfo sets;
    VkDescriptorSetLayout layouts[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      layouts[i] = r->pvm_set_layout;

    sets.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sets.pNext = NULL;
    sets.descriptorPool = r->common_set_pool;
    sets.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
    sets.pSetLayouts = layouts;

    OWL_CHECK(vkAllocateDescriptorSets(r->device, &sets, r->dynamic_pvm_sets));
  }

  /* write the pvm descriptor sets */
  {
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buffer;

      buffer.buffer = r->dynamic_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof(struct owl_draw_cmd_ubo);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = r->dynamic_pvm_sets[i];
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

  /* zero the offsets */
  {
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      r->dynamic_offsets[i] = 0;
  }

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_dynamic_buffer_(struct owl_renderer *r) {
  owl_u32 i;

  vkFreeDescriptorSets(r->device, r->common_set_pool,
                       OWL_ARRAY_SIZE(r->dynamic_pvm_sets),
                       r->dynamic_pvm_sets);

  vkFreeMemory(r->device, r->dynamic_memory, NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyBuffer(r->device, r->dynamic_buffers[i], NULL);
}

OWL_INTERNAL enum owl_code owl_renderer_init_garbage_(struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  r->garbage_buffers_count = 0;
  r->garbage_memories_count = 0;
  r->garbage_pvm_sets_count = 0;

#ifndef NDEBUG
  OWL_MEMSET(r->garbage_buffers, 0, sizeof(r->garbage_buffers));
  OWL_MEMSET(r->garbage_memories, 0, sizeof(r->garbage_memories));
  OWL_MEMSET(r->garbage_pvm_sets, 0, sizeof(r->garbage_pvm_sets));
#endif /* NDEBUG */

  return code;
}

OWL_INTERNAL void owl_renderer_deinit_garbage_(struct owl_renderer *r) {
  int i;

  for (i = 0; i < r->garbage_pvm_sets_count; ++i)
    vkFreeDescriptorSets(r->device, r->common_set_pool, 1,
                         &r->garbage_pvm_sets[i]);

  r->garbage_pvm_sets_count = 0;

  for (i = 0; i < r->garbage_memories_count; ++i)
    vkFreeMemory(r->device, r->garbage_memories[i], NULL);

  r->garbage_memories_count = 0;

  for (i = 0; i < r->garbage_buffers_count; ++i)
    vkDestroyBuffer(r->device, r->garbage_buffers[i], NULL);

  r->garbage_buffers_count = 0;
}

OWL_INTERNAL enum owl_code
owl_renderer_move_dynamic_to_garbage(struct owl_renderer *r) {
  int i;
  enum owl_code code = OWL_SUCCESS;

  {
    /* previous count */
    int prev_count = r->garbage_buffers_count;
    /* new count */
    int new_count =
        r->garbage_buffers_count + OWL_RENDERER_DYNAMIC_BUFFER_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      r->garbage_buffers[prev_count + i] = r->dynamic_buffers[i];

    r->garbage_buffers_count = new_count;
  }

  {
    /* previous count */
    int prev_count = r->garbage_pvm_sets_count;
    /* new count */
    int new_count =
        r->garbage_pvm_sets_count + OWL_RENDERER_DYNAMIC_BUFFER_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      r->garbage_pvm_sets[prev_count + i] = r->dynamic_pvm_sets[i];

    r->garbage_pvm_sets_count = new_count;
  }

  {
    /* previous count */
    int prev_count = r->garbage_memories_count;
    /* new count */
    int new_count = r->garbage_memories_count + 1;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    r->garbage_memories[prev_count + 0] = r->dynamic_memory;
    r->garbage_memories_count = new_count;
  }

end:
  return code;
}

enum owl_code owl_renderer_init(struct owl_renderer_info const *info,
                                struct owl_renderer *r) {
  enum owl_code code;

  r->width = info->framebuffer_width;
  r->height = info->framebuffer_height;

  if (OWL_SUCCESS != (code = owl_renderer_init_instance_(info, r)))
    goto end;

#ifdef OWL_ENABLE_VALIDATION
  if (OWL_SUCCESS != (code = owl_renderer_init_debug_(r)))
    goto end;
#endif /* OWL_ENABLE_VALIDATION */

  if (OWL_SUCCESS != (code = owl_renderer_init_surface_(info, r)))
    goto end_err_deinit_instance;

  if (OWL_SUCCESS != (code = owl_renderer_init_device_(r)))
    goto end_err_deinit_surface;

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_(info, r)))
    goto end_err_deinit_device;

  if (OWL_SUCCESS != (code = owl_renderer_init_common_pools_(r)))
    goto end_err_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_renderer_init_main_render_pass_(r)))
    goto end_err_deinit_common_pools;

  if (OWL_SUCCESS != (code = owl_renderer_init_attachments_(r)))
    goto end_err_deinit_main_render_pass;

  if (OWL_SUCCESS != (code = owl_renderer_init_framebuffers_(r)))
    goto end_err_deinit_attachments;

  if (OWL_SUCCESS != (code = owl_renderer_init_set_layouts_(r)))
    goto end_err_deinit_framebuffers;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipelines_layouts_(r)))
    goto end_err_deinit_set_layouts;

  if (OWL_SUCCESS != (code = owl_renderer_init_shaders_(r)))
    goto end_err_deinit_pipeline_layouts;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipelines_(r)))
    goto end_err_deinit_shaders;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_cmds_(r)))
    goto end_err_deinit_pipelines;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_sync_(r)))
    goto end_err_deinit_frame_cmds;

  if (OWL_SUCCESS != (code = owl_renderer_init_dynamic_buffer_(r, 1024 * 1024)))
    goto end_err_deinit_frame_sync;

  if (OWL_SUCCESS != (code = owl_renderer_init_garbage_(r)))
    goto end_err_deinit_dynamic_buffer;

  goto end;

end_err_deinit_dynamic_buffer:
  owl_renderer_deinit_dynamic_buffer_(r);

end_err_deinit_frame_sync:
  owl_renderer_deinit_frame_sync_(r);

end_err_deinit_frame_cmds:
  owl_renderer_deinit_frame_cmds_(r);

end_err_deinit_pipelines:
  owl_renderer_deinit_pipelines(r);

end_err_deinit_shaders:
  owl_renderer_deinit_shaders_(r);

end_err_deinit_pipeline_layouts:
  owl_renderer_deinit_pipeline_layouts_(r);

end_err_deinit_set_layouts:
  owl_renderer_deinit_set_layouts_(r);

end_err_deinit_framebuffers:
  owl_renderer_deinit_framebuffers_(r);

end_err_deinit_attachments:
  owl_renderer_deinit_attachments_(r);

end_err_deinit_main_render_pass:
  owl_renderer_deinit_main_render_pass_(r);

end_err_deinit_common_pools:
  owl_renderer_deinit_common_pools_(r);

end_err_deinit_swapchain:
  owl_renderer_deinit_swapchain_(r);

end_err_deinit_device:
  owl_renderer_deinit_device_(r);

end_err_deinit_surface:
  owl_renderer_deinit_surface_(r);

end_err_deinit_instance:
  owl_renderer_deinit_instance_(r);

end:
  return code;
}

void owl_renderer_deinit(struct owl_renderer *r) {
  OWL_CHECK(vkDeviceWaitIdle(r->device));

  owl_renderer_deinit_garbage_(r);
  owl_renderer_deinit_dynamic_buffer_(r);
  owl_renderer_deinit_frame_sync_(r);
  owl_renderer_deinit_frame_cmds_(r);
  owl_renderer_deinit_pipelines(r);
  owl_renderer_deinit_shaders_(r);
  owl_renderer_deinit_pipeline_layouts_(r);
  owl_renderer_deinit_set_layouts_(r);
  owl_renderer_deinit_framebuffers_(r);
  owl_renderer_deinit_attachments_(r);
  owl_renderer_deinit_main_render_pass_(r);
  owl_renderer_deinit_common_pools_(r);
  owl_renderer_deinit_swapchain_(r);
  owl_renderer_deinit_device_(r);
  owl_renderer_deinit_surface_(r);

#ifdef OWL_ENABLE_VALIDATION
  owl_renderer_deinit_debug_(r);
#endif /* OWL_ENABLE_VALIDATION */

  owl_renderer_deinit_instance_(r);
}

enum owl_code owl_renderer_reserve_dynamic_memory(struct owl_renderer *r,
                                                  VkDeviceSize size) {
  enum owl_code code = OWL_SUCCESS;
  VkDeviceSize required = r->dynamic_offsets[r->active] + size;

  if (required > r->dynamic_buffer_size) {
    vkUnmapMemory(r->device, r->dynamic_memory);

    if (OWL_SUCCESS != (code = owl_renderer_move_dynamic_to_garbage(r)))
      goto end;

    if (OWL_SUCCESS !=
        (code = owl_renderer_init_dynamic_buffer_(r, 2 * required)))
      goto end;
  }

end:
  return code;
}

enum owl_code
owl_renderer_reinit_swapchain(struct owl_renderer_info const *info,
                              struct owl_renderer *r) {
  enum owl_code code = OWL_SUCCESS;

  OWL_CHECK(vkDeviceWaitIdle(r->device));

  owl_renderer_deinit_pipelines(r);
  owl_renderer_deinit_framebuffers_(r);
  owl_renderer_deinit_attachments_(r);
  owl_renderer_deinit_main_render_pass_(r);
  owl_renderer_deinit_frame_sync_(r);
  owl_renderer_deinit_swapchain_(r);

  if (OWL_SUCCESS != (code = owl_renderer_init_swapchain_(info, r)))
    goto end;

  if (OWL_SUCCESS != (code = owl_renderer_init_frame_sync_(r)))
    goto end_err_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_renderer_init_main_render_pass_(r)))
    goto end_err_deinit_frame_sync;

  if (OWL_SUCCESS != (code = owl_renderer_init_attachments_(r)))
    goto end_err_deinit_main_render_pass;

  if (OWL_SUCCESS != (code = owl_renderer_init_framebuffers_(r)))
    goto end_err_deinit_main_attachments;

  if (OWL_SUCCESS != (code = owl_renderer_init_pipelines_(r)))
    goto end_err_deinit_main_framebuffers;

  goto end;

end_err_deinit_main_framebuffers:
  owl_renderer_deinit_framebuffers_(r);

end_err_deinit_main_attachments:
  owl_renderer_deinit_attachments_(r);

end_err_deinit_main_render_pass:
  owl_renderer_deinit_main_render_pass_(r);

end_err_deinit_frame_sync:
  owl_renderer_deinit_frame_sync_(r);

end_err_deinit_swapchain:
  owl_renderer_deinit_swapchain_(r);

end:
  return code;
}

int owl_renderer_is_dynamic_buffer_clear(struct owl_renderer *r) {
  return 0 == r->dynamic_offsets[r->active];
}

void owl_renderer_clear_garbage(struct owl_renderer *r) {
  owl_renderer_deinit_garbage_(r);

  /* NOTE: deiniting dyn garbage leaves it in a valid state */
#if 0
  owl_renderer_init_garbage_(r);
#endif
}

void owl_renderer_clear_dynamic_offset(struct owl_renderer *r) {
  r->dynamic_offsets[r->active] = 0;
}

enum owl_code owl_renderer_create(struct owl_window *w,
                                  struct owl_renderer **r) {
  struct owl_renderer_info info;
  enum owl_code code = OWL_SUCCESS;

  if (!(*r = OWL_MALLOC(sizeof(**r)))) {
    code = OWL_ERROR_BAD_ALLOC;
    goto end;
  }

  owl_window_fill_renderer_info(w, &info);

  if (OWL_SUCCESS != (code = owl_renderer_init(&info, *r))) {
    OWL_FREE(*r);
    goto end;
  }

end:
  return code;
}

enum owl_code owl_renderer_recreate_swapchain(struct owl_window *w,
                                              struct owl_renderer *r) {
  struct owl_renderer_info info;
  owl_window_fill_renderer_info(w, &info);
  return owl_renderer_reinit_swapchain(&info, r);
}

void owl_renderer_destroy(struct owl_renderer *r) {
  owl_renderer_deinit(r);
  OWL_FREE(r);
}

OWL_INTERNAL VkDeviceSize
owl_renderer_next_dynamic_offset_(struct owl_renderer *r, VkDeviceSize size) {
  return OWL_ALIGNU2(r->dynamic_offsets[r->active] + size,
                     r->dynamic_buffer_alignment);
}

void *
owl_renderer_dynamic_buffer_alloc(struct owl_renderer *r, VkDeviceSize size,
                                  struct owl_dynamic_buffer_reference *ref) {
  owl_byte *data = NULL;

  if (OWL_SUCCESS != owl_renderer_reserve_dynamic_memory(r, size))
    goto end;

  ref->offset32 = (owl_u32)r->dynamic_offsets[r->active];
  ref->offset = r->dynamic_offsets[r->active];
  ref->buffer = r->dynamic_buffers[r->active];
  ref->pvm_set = r->dynamic_pvm_sets[r->active];

  data = r->dynamic_data[r->active] + r->dynamic_offsets[r->active];
  r->dynamic_offsets[r->active] = owl_renderer_next_dynamic_offset_(r, size);

end:
  return data;
}

OWL_INTERNAL VkMemoryPropertyFlags
owl_as_vk_memory_property_flags(enum owl_memory_visibility vis) {
  switch (vis) {
  case OWL_MEMORY_VISIBILITY_CPU_ONLY:
  case OWL_MEMORY_VISIBILITY_CPU_TO_GPU:
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  case OWL_MEMORY_VISIBILITY_GPU_ONLY:
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
}

owl_u32 owl_renderer_find_memory_type(struct owl_renderer const *r,
                                      owl_u32 filter,
                                      enum owl_memory_visibility vis) {
  owl_u32 i;
  VkMemoryPropertyFlags property = owl_as_vk_memory_property_flags(vis);

  for (i = 0; i < r->device_memory_properties.memoryTypeCount; ++i) {
    owl_u32 f = r->device_memory_properties.memoryTypes[i].propertyFlags;

    if ((f & property) && (filter & (1U << i)))
      return i;
  }

  return OWL_MEMORY_TYPE_NONE;
}

enum owl_code owl_renderer_bind_pipeline(struct owl_renderer *r,
                                         enum owl_pipeline_type type) {
  enum owl_code code = OWL_SUCCESS;
  r->bound_pipeline = type;

  if (OWL_PIPELINE_TYPE_NONE == type)
    goto end;

  vkCmdBindPipeline(r->frame_cmd_buffers[r->active],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipelines[type]);

end:
  return code;
}

enum owl_code
owl_renderer_alloc_single_use_cmd_buffer(struct owl_renderer const *r,
                                         VkCommandBuffer *cmd) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkCommandBufferAllocateInfo buf;

    buf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buf.pNext = NULL;
    buf.commandPool = r->transient_cmd_pool;
    buf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buf.commandBufferCount = 1;

    OWL_CHECK(vkAllocateCommandBuffers(r->device, &buf, cmd));
  }

  {
    VkCommandBufferBeginInfo begin;

    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.pNext = NULL;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin.pInheritanceInfo = NULL;

    OWL_CHECK(vkBeginCommandBuffer(*cmd, &begin));
  }

  return code;
}

enum owl_code
owl_renderer_free_single_use_cmd_buffer(struct owl_renderer const *r,
                                        VkCommandBuffer cmd) {
  VkSubmitInfo submit;
  enum owl_code code = OWL_SUCCESS;

  OWL_CHECK(vkEndCommandBuffer(cmd));

  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.pNext = NULL;
  submit.waitSemaphoreCount = 0;
  submit.pWaitSemaphores = NULL;
  submit.pWaitDstStageMask = NULL;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  submit.signalSemaphoreCount = 0;
  submit.pSignalSemaphores = NULL;

  OWL_CHECK(vkQueueSubmit(r->graphics_queue, 1, &submit, NULL));
  OWL_CHECK(vkQueueWaitIdle(r->graphics_queue));

  vkFreeCommandBuffers(r->device, r->transient_cmd_pool, 1, &cmd);

  return code;
}

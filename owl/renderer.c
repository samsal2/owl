#include "renderer.h"

#include "internal.h"
#include "model.h"
#include "window.h"

/* clang-format off */
#define OWL_VK_DEVICE_EXTENSIONS { VK_KHR_SWAPCHAIN_EXTENSION_NAME }
/* clang-format on */

#ifndef NDEBUG

#define OWL_VK_GET_INSTANCE_PROC_ADDR(i, fn)                                   \
  ((PFN_##fn)vkGetInstanceProcAddr((i), #fn))

OWL_GLOBAL char const *const g_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32 owl_vk_debug_cb_(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user_data) {
  OWL_UNUSED(severity);
  OWL_UNUSED(type);
  OWL_UNUSED(user_data);
  fprintf(stderr, "%s\n", data->pMessage);
  return VK_FALSE;
}

OWL_INTERNAL enum owl_code
owl_init_instance_(struct owl_vk_plataform const *plataform,
                   struct owl_vk_renderer *renderer) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = "OWL";
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pNext = NULL;
  instance.flags = 0;
  instance.pApplicationInfo = &app;
  instance.enabledLayerCount = OWL_ARRAY_SIZE(g_validation_layers);
  instance.ppEnabledLayerNames = g_validation_layers;
  instance.enabledExtensionCount = (owl_u32)plataform->instance_extension_count;
  instance.ppEnabledExtensionNames = plataform->instance_extensions;

  OWL_VK_CHECK(vkCreateInstance(&instance, NULL, &renderer->instance));

  return OWL_SUCCESS;
}

OWL_INTERNAL enum owl_code owl_init_debug_(struct owl_vk_renderer *renderer) {
  VkDebugUtilsMessengerCreateInfoEXT debug;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;

  vkCreateDebugUtilsMessengerEXT = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkCreateDebugUtilsMessengerEXT);

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
  debug.pfnUserCallback = owl_vk_debug_cb_;
  debug.pUserData = NULL;

#undef OWL_DEBUG_SEVERITY_FLAGS
#undef OWL_DEBUG_TYPE_FLAGS

  OWL_VK_CHECK(vkCreateDebugUtilsMessengerEXT(renderer->instance, &debug, NULL,
                                              &renderer->debug));
  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_debug_(struct owl_vk_renderer *renderer) {
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger;

  vkDestroyDebugUtilsMessenger = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkDestroyDebugUtilsMessengerEXT);

  OWL_ASSERT(vkDestroyDebugUtilsMessenger);

  vkDestroyDebugUtilsMessenger(renderer->instance, renderer->debug, NULL);
}

#else /* NDEBUG */

OWL_INTERNAL enum owl_code
owl_init_instance_(struct owl_vk_plataform const *plataform,
                   struct owl_vk_renderer *renderer) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = "Owl";
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pNext = NULL;
  instance.flags = 0;
  instance.pApplicationInfo = &app;
  instance.enabledLayerCount = 0;
  instance.ppEnabledLayerNames = NULL;
  instance.enabledExtensionCount = (OwlU32)plataform->instance_extension_count;
  instance.ppEnabledExtensionNames = plataform->instance_extensions;

  OWL_VK_CHECK(vkCreateInstance(&instance, NULL, &renderer->instance));

  return OWL_SUCCESS;
}

#endif /* NDBUG */

OWL_INTERNAL void owl_deinit_instance_(struct owl_vk_renderer *renderer) {
  vkDestroyInstance(renderer->instance, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_surface_(struct owl_vk_plataform const *plataform,
                  struct owl_vk_renderer *renderer) {
  return plataform->create_surface(renderer, plataform->surface_user_data,
                                   &renderer->surface);
}

OWL_INTERNAL void owl_deinit_surface_(struct owl_vk_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

OWL_INTERNAL enum owl_code
owl_fill_device_options_(struct owl_vk_renderer *renderer) {
  OWL_VK_CHECK(vkEnumeratePhysicalDevices(
      renderer->instance, &renderer->device_options_count, NULL));

  if (OWL_RENDERER_MAX_DEVICE_OPTIONS <= renderer->device_options_count)
    return OWL_ERROR_OUT_OF_SPACE;

  OWL_VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance,
                                          &renderer->device_options_count,
                                          renderer->device_options));

  return OWL_SUCCESS;
}

OWL_INTERNAL int owl_vk_query_families(struct owl_vk_renderer const *renderer,
                                       VkPhysicalDevice device,
                                       owl_u32 *graphics_family,
                                       owl_u32 *present_family) {
#define OWL_QUEUE_UNSELECTED (owl_u32) - 1
  int found;
  owl_u32 i, count;
  VkQueueFamilyProperties *props;

  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);

  if (!(props = OWL_MALLOC(count * sizeof(*props))))
    return 0;

  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props);

  *graphics_family = OWL_QUEUE_UNSELECTED;
  *present_family = OWL_QUEUE_UNSELECTED;

  for (i = 0; i < count; ++i) {
    VkBool32 surface;

    if (!props[i].queueCount)
      continue;

    if (VK_QUEUE_GRAPHICS_BIT & props[i].queueFlags)
      *graphics_family = i;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
        device, i, renderer->surface, &surface));

    if (surface)
      *present_family = i;

    if (OWL_QUEUE_UNSELECTED == *graphics_family)
      continue;

    if (OWL_QUEUE_UNSELECTED == *present_family)
      continue;

    found = 1;
    goto end_free_props;
  }

  found = 0;

end_free_props:
  OWL_FREE(props);

  return found;
}

OWL_INTERNAL int
owl_vk_validate_extensions(owl_u32 count,
                           VkExtensionProperties const *extensions) {
  char const *validate[] = OWL_VK_DEVICE_EXTENSIONS;
  owl_u32 i, j, found[OWL_ARRAY_SIZE(validate)];

  OWL_MEMSET(found, 0, sizeof(found));

  for (i = 0; i < count; ++i)
    for (j = 0; j < OWL_ARRAY_SIZE(validate); ++j)
      if (!strcmp(validate[j], extensions[i].extensionName))
        found[j] = 1;

  for (i = 0; i < OWL_ARRAY_SIZE(validate); ++i)
    if (!found[i])
      return 0;

  return 1;
}

OWL_INTERNAL enum owl_code
owl_select_physical_device_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < renderer->device_options_count; ++i) {
    owl_u32 formats, modes;
    owl_u32 extension_count;
    VkExtensionProperties *extensions;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        renderer->device_options[i], renderer->surface, &formats, NULL));

    if (!formats)
      continue;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        renderer->device_options[i], renderer->surface, &modes, NULL));

    if (!modes)
      continue;

    if (!owl_vk_query_families(renderer, renderer->device_options[i],
                               &renderer->graphics_family,
                               &renderer->present_family))
      continue;

    vkGetPhysicalDeviceFeatures(renderer->device_options[i],
                                &renderer->device_features);

    if (!renderer->device_features.samplerAnisotropy)
      continue;

    OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
        renderer->device_options[i], NULL, &extension_count, NULL));

    if (!(extensions = OWL_MALLOC(extension_count * sizeof(*extensions))))
      return OWL_ERROR_BAD_ALLOC;

    OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
        renderer->device_options[i], NULL, &extension_count, extensions));

    if (!owl_vk_validate_extensions(extension_count, extensions)) {
      OWL_FREE(extensions);
      continue;
    }

    OWL_FREE(extensions);

    renderer->physical_device = renderer->device_options[i];

    vkGetPhysicalDeviceProperties(renderer->physical_device,
                                  &renderer->device_properties);

    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device,
                                        &renderer->device_mem_properties);

    return OWL_SUCCESS;
  }

  return OWL_ERROR_NO_SUITABLE_DEVICE;
}

OWL_INTERNAL enum owl_code
owl_select_surface_format_(struct owl_vk_renderer *renderer, VkFormat format,
                           VkColorSpaceKHR color_space) {
  owl_u32 i, count;
  VkSurfaceFormatKHR *formats;
  enum owl_code code = OWL_SUCCESS;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &count, NULL));

  if (!(formats = OWL_MALLOC(count * sizeof(*formats))))
    return OWL_ERROR_BAD_ALLOC;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
      renderer->physical_device, renderer->surface, &count, formats));

  for (i = 0; i < count; ++i) {
    if (format != formats[i].format)
      continue;

    if (color_space != formats[i].colorSpace)
      continue;

    renderer->surface_format.format = format;
    renderer->surface_format.colorSpace = color_space;

    code = OWL_SUCCESS;
    goto end_free_formats;
  }

  /* if the loop ends without finding a format */
  code = OWL_ERROR_NO_SUITABLE_FORMAT;

end_free_formats:
  OWL_FREE(formats);

  return code;
}

OWL_INTERNAL enum owl_code
owl_select_sample_count_(struct owl_vk_renderer *renderer) {
  VkSampleCountFlags const samples =
      renderer->device_properties.limits.framebufferColorSampleCounts &
      renderer->device_properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_2_BIT & samples) {
    renderer->samples = VK_SAMPLE_COUNT_2_BIT;
  } else {
    OWL_ASSERT(0 && "disabling multisampling is not supported");
    renderer->samples = VK_SAMPLE_COUNT_1_BIT;
    return OWL_ERROR_UNKNOWN;
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL owl_u32
owl_vk_queue_count(struct owl_vk_renderer const *renderer) {
  return renderer->graphics_family == renderer->present_family ? 1 : 2;
}

OWL_INTERNAL enum owl_code owl_init_device_(struct owl_vk_renderer *renderer) {
  VkDeviceCreateInfo device;
  VkDeviceQueueCreateInfo queues[2];
  float const prio = 1.0F;
  char const *extensions[] = OWL_VK_DEVICE_EXTENSIONS;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_SUCCESS != (code = owl_fill_device_options_(renderer)))
    goto end;

  if (OWL_SUCCESS != (code = owl_select_physical_device_(renderer)))
    goto end;

  if (OWL_SUCCESS !=
      (code = owl_select_surface_format_(renderer, VK_FORMAT_B8G8R8A8_SRGB,
                                         VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)))
    goto end;

  if (OWL_SUCCESS != (code = owl_select_sample_count_(renderer)))
    goto end;

  queues[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues[0].pNext = NULL;
  queues[0].flags = 0;
  queues[0].queueFamilyIndex = renderer->graphics_family;
  queues[0].queueCount = 1;
  queues[0].pQueuePriorities = &prio;

  queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queues[1].pNext = NULL;
  queues[1].flags = 0;
  queues[1].queueFamilyIndex = renderer->present_family;
  queues[1].queueCount = 1;
  queues[1].pQueuePriorities = &prio;

  device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device.pNext = NULL;
  device.flags = 0;
  device.queueCreateInfoCount = owl_vk_queue_count(renderer);
  device.pQueueCreateInfos = queues;
  device.enabledLayerCount = 0;      /* deprecated */
  device.ppEnabledLayerNames = NULL; /* deprecated */
  device.enabledExtensionCount = OWL_ARRAY_SIZE(extensions);
  device.ppEnabledExtensionNames = extensions;
  device.pEnabledFeatures = &renderer->device_features;

  OWL_VK_CHECK(vkCreateDevice(renderer->physical_device, &device, NULL,
                              &renderer->device));

  vkGetDeviceQueue(renderer->device, renderer->graphics_family, 0,
                   &renderer->graphics_queue);

  vkGetDeviceQueue(renderer->device, renderer->present_family, 0,
                   &renderer->present_queue);
end:
  return code;
}

OWL_INTERNAL void owl_deinit_device_(struct owl_vk_renderer *renderer) {
  vkDestroyDevice(renderer->device, NULL);
}

OWL_INTERNAL void
owl_clamp_swapchain_extent_(owl_u32 width, owl_u32 height,
                            struct owl_vk_renderer *renderer) {
#define OWL_VK_NO_RESTRICTIONS (owl_u32) - 1

  VkSurfaceCapabilitiesKHR capabilities;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      renderer->physical_device, renderer->surface, &capabilities));

  if (OWL_VK_NO_RESTRICTIONS == capabilities.currentExtent.width) {
    renderer->swapchain_extent = capabilities.currentExtent;
  } else {
    owl_u32 min_width = capabilities.minImageExtent.width;
    owl_u32 min_height = capabilities.minImageExtent.height;
    owl_u32 max_width = capabilities.maxImageExtent.width;
    owl_u32 max_height = capabilities.maxImageExtent.height;

    renderer->swapchain_extent.width = OWL_CLAMP(width, min_width, max_width);
    renderer->swapchain_extent.height =
        OWL_CLAMP(height, min_height, max_height);
  }

#undef OWL_VK_NO_RESTRICTIONS
}

OWL_INTERNAL enum owl_code
owl_select_present_mode_(struct owl_vk_renderer *renderer,
                         VkPresentModeKHR mode) {
#define OWL_MAX_PRESENT_MODES 16
  owl_u32 i, count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &count, NULL));

  if (OWL_MAX_PRESENT_MODES <= count)
    return OWL_ERROR_OUT_OF_SPACE;

  OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
      renderer->physical_device, renderer->surface, &count, modes));

  /* fifo is guaranteed */
  renderer->swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

  for (i = 0; i < count; ++i)
    if (mode == (renderer->swapchain_present_mode = modes[count - i - 1]))
      break;

  return OWL_SUCCESS;
#undef OWL_MAX_PRESENT_MODES
}

OWL_INTERNAL enum owl_code
owl_init_swapchain_(struct owl_vk_plataform const *plataform,
                    struct owl_vk_renderer *renderer) {
  owl_u32 families[2];
  enum owl_code code = OWL_SUCCESS;

  owl_clamp_swapchain_extent_((owl_u32)plataform->framebuffer_width,
                              (owl_u32)plataform->framebuffer_height, renderer);

  if (OWL_SUCCESS !=
      (code = owl_select_present_mode_(renderer, VK_PRESENT_MODE_FIFO_KHR)))
    goto end;

  {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSwapchainCreateInfoKHR swapchain;

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        renderer->physical_device, renderer->surface, &capabilities));

    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.pNext = NULL;
    swapchain.flags = 0;
    swapchain.surface = renderer->surface;
    swapchain.minImageCount = 2;
    swapchain.imageFormat = renderer->surface_format.format;
    swapchain.imageColorSpace = renderer->surface_format.colorSpace;
    swapchain.imageExtent.width = renderer->swapchain_extent.width;
    swapchain.imageExtent.height = renderer->swapchain_extent.height;
    swapchain.imageArrayLayers = 1;
    swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain.preTransform = capabilities.currentTransform;
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain.presentMode = renderer->swapchain_present_mode;
    swapchain.clipped = VK_TRUE;
    swapchain.oldSwapchain = NULL;

    if (renderer->graphics_family == renderer->present_family) {
      swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain.queueFamilyIndexCount = 0;
      swapchain.pQueueFamilyIndices = NULL;
    } else {
      families[0] = renderer->graphics_family;
      families[1] = renderer->present_family;

      swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain.queueFamilyIndexCount = owl_vk_queue_count(renderer);
      swapchain.pQueueFamilyIndices = families;
    }

    OWL_VK_CHECK(vkCreateSwapchainKHR(renderer->device, &swapchain, NULL,
                                      &renderer->swapchain));
  }

  {
    OWL_VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                         &renderer->swapchain_imgs_count,
                                         NULL));

    OWL_ASSERT(OWL_RENDERER_MAX_SWAPCHAIN_IMAGES >
               renderer->swapchain_imgs_count);

    OWL_VK_CHECK(vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                         &renderer->swapchain_imgs_count,
                                         renderer->swapchain_imgs));
  }

  {
    owl_u32 i;
    for (i = 0; i < renderer->swapchain_imgs_count; ++i) {
      VkImageViewCreateInfo view;

      view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view.pNext = NULL;
      view.flags = 0;
      view.image = renderer->swapchain_imgs[i];
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view.format = renderer->surface_format.format;
      view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.baseMipLevel = 0;
      view.subresourceRange.levelCount = 1;
      view.subresourceRange.baseArrayLayer = 0;
      view.subresourceRange.layerCount = 1;

      OWL_VK_CHECK(vkCreateImageView(renderer->device, &view, NULL,
                                     &renderer->swapchain_views[i]));
    }
  }

  renderer->swapchain_clear_vals[0].color.float32[0] = 0.0F;
  renderer->swapchain_clear_vals[0].color.float32[1] = 0.0F;
  renderer->swapchain_clear_vals[0].color.float32[2] = 0.0F;
  renderer->swapchain_clear_vals[0].color.float32[3] = 1.0F;
  renderer->swapchain_clear_vals[1].depthStencil.depth = 1.0F;
  renderer->swapchain_clear_vals[1].depthStencil.stencil = 0.0F;

end:
  return code;
}

OWL_INTERNAL void owl_deinit_swapchain_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < renderer->swapchain_imgs_count; ++i)
    vkDestroyImageView(renderer->device, renderer->swapchain_views[i], NULL);

  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_general_pools_(struct owl_vk_renderer *renderer) {
  {
    VkCommandPoolCreateInfo pool;

    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.pNext = NULL;
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = renderer->graphics_family;

    OWL_VK_CHECK(vkCreateCommandPool(renderer->device, &pool, NULL,
                                     &renderer->transient_cmd_pool));
  }

  {
#define OWL_MAX_UNIFORM_SETS 16
#define OWL_MAX_SAMPLER_SETS 16
#define OWL_MAX_SETS 32
    VkDescriptorPoolSize sizes[2];
    VkDescriptorPoolCreateInfo pool;

    sizes[0].descriptorCount = OWL_MAX_UNIFORM_SETS;
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    sizes[1].descriptorCount = OWL_MAX_SAMPLER_SETS;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool.pNext = NULL;
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = OWL_MAX_SETS;
    pool.poolSizeCount = OWL_ARRAY_SIZE(sizes);
    pool.pPoolSizes = sizes;

    OWL_VK_CHECK(vkCreateDescriptorPool(renderer->device, &pool, NULL,
                                        &renderer->set_pool));
#undef OWL_MAX_SETS
#undef OWL_MAX_SAMPLER_SETS
#undef OWL_MAX_UNIFORM_SETS
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_general_pools_(struct owl_vk_renderer *renderer) {
  vkDestroyDescriptorPool(renderer->device, renderer->set_pool, NULL);
  vkDestroyCommandPool(renderer->device, renderer->transient_cmd_pool, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_frame_cmds_(struct owl_vk_renderer *renderer) {
  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkCommandPoolCreateInfo pool;

      pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      pool.pNext = NULL;
      pool.flags = 0;
      pool.queueFamilyIndex = renderer->graphics_family;

      OWL_VK_CHECK(vkCreateCommandPool(renderer->device, &pool, NULL,
                                       &renderer->frame_cmd_pools[i]));
    }
  }

  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkCommandBufferAllocateInfo buf;

      buf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      buf.pNext = NULL;
      buf.commandPool = renderer->frame_cmd_pools[i];
      buf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      buf.commandBufferCount = 1;

      OWL_VK_CHECK(vkAllocateCommandBuffers(renderer->device, &buf,
                                            &renderer->frame_cmd_bufs[i]));
    }
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_frame_cmds_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkFreeCommandBuffers(renderer->device, renderer->frame_cmd_pools[i], 1,
                         &renderer->frame_cmd_bufs[i]);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyCommandPool(renderer->device, renderer->frame_cmd_pools[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_frame_sync_(struct owl_vk_renderer *renderer) {
  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkSemaphoreCreateInfo semaphore;
      semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      semaphore.pNext = NULL;
      semaphore.flags = 0;
      OWL_VK_CHECK(vkCreateSemaphore(renderer->device, &semaphore, NULL,
                                     &renderer->frame_img_available[i]));
    }
  }

  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkSemaphoreCreateInfo semaphore;
      semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      semaphore.pNext = NULL;
      semaphore.flags = 0;
      OWL_VK_CHECK(vkCreateSemaphore(renderer->device, &semaphore, NULL,
                                     &renderer->frame_render_done[i]));
    }
  }

  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkFenceCreateInfo fence;
      fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence.pNext = NULL;
      fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
      OWL_VK_CHECK(vkCreateFence(renderer->device, &fence, NULL,
                                 &renderer->frame_in_flight[i]));
    }
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_frame_sync_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(renderer->device, renderer->frame_img_available[i],
                       NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(renderer->device, renderer->frame_render_done[i], NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyFence(renderer->device, renderer->frame_in_flight[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_render_pass_(struct owl_vk_renderer *renderer) {
  VkAttachmentReference refs[3];
  VkAttachmentDescription atts[3];
  VkSubpassDescription subpass;
  VkSubpassDependency deps;
  VkRenderPassCreateInfo pass;

  refs[0].attachment = 0;
  refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  refs[1].attachment = 1;
  refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  refs[2].attachment = 2;
  refs[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  atts[0].flags = 0;
  atts[0].format = renderer->surface_format.format;
  atts[0].samples = renderer->samples;
  atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  atts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  atts[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  atts[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  atts[1].flags = 0;
  atts[1].format = VK_FORMAT_D32_SFLOAT;
  atts[1].samples = renderer->samples;
  atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  atts[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  atts[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  atts[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  atts[2].flags = 0;
  atts[2].format = renderer->surface_format.format;
  atts[2].samples = VK_SAMPLE_COUNT_1_BIT;
  atts[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  atts[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  atts[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  atts[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  atts[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  atts[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &refs[0];
  subpass.pResolveAttachments = &refs[2];
  subpass.pDepthStencilAttachment = &refs[1];
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

  deps.srcSubpass = VK_SUBPASS_EXTERNAL;
  deps.dstSubpass = 0;
  deps.srcStageMask = OWL_SRC_STAGE;
  deps.srcAccessMask = 0;
  deps.dstStageMask = OWL_DST_STAGE;
  deps.dstAccessMask = OWL_DST_ACCESS;
  deps.dependencyFlags = 0;

#undef OWL_DST_ACCESS
#undef OWL_DST_STAGE
#undef OWL_SRC_STAGE

  pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass.pNext = NULL;
  pass.flags = 0;
  pass.attachmentCount = OWL_ARRAY_SIZE(atts);
  pass.pAttachments = atts;
  pass.subpassCount = 1;
  pass.pSubpasses = &subpass;
  pass.dependencyCount = 1;
  pass.pDependencies = &deps;

  OWL_VK_CHECK(vkCreateRenderPass(renderer->device, &pass, NULL,
                                  &renderer->main_render_pass));

  return OWL_SUCCESS;
}

OWL_INTERNAL void
owl_deinit_main_render_pass_(struct owl_vk_renderer *renderer) {
  vkDestroyRenderPass(renderer->device, renderer->main_render_pass, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_attachments_(struct owl_vk_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    VkImageCreateInfo img;
    img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img.pNext = NULL;
    img.flags = 0;
    img.imageType = VK_IMAGE_TYPE_2D;
    img.format = renderer->surface_format.format;
    img.extent.width = renderer->swapchain_extent.width;
    img.extent.height = renderer->swapchain_extent.height;
    img.extent.depth = 1;
    img.mipLevels = 1;
    img.arrayLayers = 1;
    img.samples = renderer->samples;
    img.tiling = VK_IMAGE_TILING_OPTIMAL;
    img.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img.queueFamilyIndexCount = 0;
    img.pQueueFamilyIndices = NULL;
    img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(
        vkCreateImage(renderer->device, &img, NULL, &renderer->color_img));
  }

  {
    VkMemoryRequirements req;
    VkMemoryAllocateInfo mem;
    vkGetImageMemoryRequirements(renderer->device, renderer->color_img, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &renderer->color_mem));

    OWL_VK_CHECK(vkBindImageMemory(renderer->device, renderer->color_img,
                                   renderer->color_mem, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = renderer->color_img;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = renderer->surface_format.format;
    view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = 1;

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view, NULL,
                                   &renderer->color_view));
  }

  {
    VkImageCreateInfo img;
    img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img.pNext = NULL;
    img.flags = 0;
    img.imageType = VK_IMAGE_TYPE_2D;
    img.format = VK_FORMAT_D32_SFLOAT;
    img.extent.width = renderer->swapchain_extent.width;
    img.extent.height = renderer->swapchain_extent.height;
    img.extent.depth = 1;
    img.mipLevels = 1;
    img.arrayLayers = 1;
    img.samples = renderer->samples;
    img.tiling = VK_IMAGE_TILING_OPTIMAL;
    img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    img.queueFamilyIndexCount = 0;
    img.pQueueFamilyIndices = NULL;
    img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OWL_VK_CHECK(
        vkCreateImage(renderer->device, &img, NULL, &renderer->depth_img));
  }

  {
    VkMemoryRequirements req;
    VkMemoryAllocateInfo mem;
    vkGetImageMemoryRequirements(renderer->device, renderer->depth_img, &req);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize = req.size;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_GPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &renderer->depth_mem));

    OWL_VK_CHECK(vkBindImageMemory(renderer->device, renderer->depth_img,
                                   renderer->depth_mem, 0));
  }

  {
    VkImageViewCreateInfo view;
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.flags = 0;
    view.image = renderer->depth_img;
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

    OWL_VK_CHECK(vkCreateImageView(renderer->device, &view, NULL,
                                   &renderer->depth_view));
  }

  return code;
}

OWL_INTERNAL void
owl_deinit_main_attachments_(struct owl_vk_renderer *renderer) {
  vkDestroyImageView(renderer->device, renderer->depth_view, NULL);
  vkFreeMemory(renderer->device, renderer->depth_mem, NULL);
  vkDestroyImage(renderer->device, renderer->depth_img, NULL);

  vkDestroyImageView(renderer->device, renderer->color_view, NULL);
  vkFreeMemory(renderer->device, renderer->color_mem, NULL);
  vkDestroyImage(renderer->device, renderer->color_img, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_framebuffers_(struct owl_vk_renderer *renderer) {
  owl_u32 i;
  for (i = 0; i < renderer->swapchain_imgs_count; ++i) {
    VkImageView atts[3];
    VkFramebufferCreateInfo framebuffer;

    atts[0] = renderer->color_view;
    atts[1] = renderer->depth_view;
    atts[2] = renderer->swapchain_views[i];

    framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.pNext = NULL;
    framebuffer.flags = 0;
    framebuffer.renderPass = renderer->main_render_pass;
    framebuffer.attachmentCount = OWL_ARRAY_SIZE(atts);
    framebuffer.pAttachments = atts;
    framebuffer.width = renderer->swapchain_extent.width;
    framebuffer.height = renderer->swapchain_extent.height;
    framebuffer.layers = 1;

    OWL_VK_CHECK(vkCreateFramebuffer(renderer->device, &framebuffer, NULL,
                                     &renderer->framebuffers[i]));
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void
owl_deinit_main_framebuffers_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  for (i = 0; i < renderer->swapchain_imgs_count; ++i)
    vkDestroyFramebuffer(renderer->device, renderer->framebuffers[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_set_layouts_(struct owl_vk_renderer *renderer) {
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

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                             &renderer->pvm_set_layout));
  }

  {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo layout;

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.pImmutableSamplers = NULL;

    layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.bindingCount = 1;
    layout.pBindings = &binding;

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                             &renderer->tex_set_layout));
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

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                             &renderer->scene_set_layout));
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

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                             &renderer->material_set_layout));
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

    OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device, &layout, NULL,
                                             &renderer->node_set_layout));
  }

  return code;
}

OWL_INTERNAL void
owl_deinit_main_set_layouts_(struct owl_vk_renderer *renderer) {
  vkDestroyDescriptorSetLayout(renderer->device, renderer->node_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(renderer->device, renderer->material_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(renderer->device, renderer->scene_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(renderer->device, renderer->tex_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(renderer->device, renderer->pvm_set_layout,
                               NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_pipeline_layouts_(struct owl_vk_renderer *renderer) {
  {
    VkDescriptorSetLayout sets[2];
    VkPipelineLayoutCreateInfo layout;

    sets[0] = renderer->pvm_set_layout;
    sets[1] = renderer->tex_set_layout;

    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.setLayoutCount = OWL_ARRAY_SIZE(sets);
    layout.pSetLayouts = sets;
    layout.pushConstantRangeCount = 0;
    layout.pPushConstantRanges = NULL;

    OWL_VK_CHECK(vkCreatePipelineLayout(renderer->device, &layout, NULL,
                                        &renderer->main_pipeline_layout));
  }

  {
    VkPushConstantRange range;
    VkDescriptorSetLayout sets[3];
    VkPipelineLayoutCreateInfo layout;

    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset = 0;
    range.size = sizeof(struct owl_material_push_constant);

    sets[0] = renderer->scene_set_layout;
    sets[1] = renderer->material_set_layout;
    sets[2] = renderer->node_set_layout;

    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pNext = NULL;
    layout.flags = 0;
    layout.setLayoutCount = OWL_ARRAY_SIZE(sets);
    layout.pSetLayouts = sets;
    layout.pushConstantRangeCount = 1;
    layout.pPushConstantRanges = &range;

    OWL_VK_CHECK(vkCreatePipelineLayout(renderer->device, &layout, NULL,
                                        &renderer->pbr_pipeline_layout));
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void
owl_deinit_main_pipeline_layouts_(struct owl_vk_renderer *renderer) {
  vkDestroyPipelineLayout(renderer->device, renderer->pbr_pipeline_layout,
                          NULL);
  vkDestroyPipelineLayout(renderer->device, renderer->main_pipeline_layout,
                          NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_shaders_(struct owl_vk_renderer *renderer) {
  {
    VkShaderModuleCreateInfo shader;

    /* TODO(samuel): Properly load code at runtime */
    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/basic_vert.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader, NULL,
                                      &renderer->basic_vert_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/basic_frag.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader, NULL,
                                      &renderer->basic_frag_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/font_frag.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader, NULL,
                                      &renderer->font_frag_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/pbr_vert.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader, NULL,
                                      &renderer->pbr_vert_shader));
  }

  {
    VkShaderModuleCreateInfo shader;

    OWL_LOCAL_PERSIST owl_u32 const code[] = {
#include "../shaders/pbr_frag.spv.u32"
    };

    shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader.pNext = NULL;
    shader.flags = 0;
    shader.codeSize = sizeof(code);
    shader.pCode = code;

    OWL_VK_CHECK(vkCreateShaderModule(renderer->device, &shader, NULL,
                                      &renderer->pbr_frag_shader));
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_main_shaders_(struct owl_vk_renderer *renderer) {
  vkDestroyShaderModule(renderer->device, renderer->pbr_frag_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->pbr_vert_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->font_frag_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_frag_shader, NULL);
  vkDestroyShaderModule(renderer->device, renderer->basic_vert_shader, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_pipelines_(struct owl_vk_renderer *renderer) {
  /* basic */
  {
    VkVertexInputBindingDescription vtx_binding;
    VkVertexInputAttributeDescription vtx_attr[3];
    VkPipelineVertexInputStateCreateInfo input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline;

    vtx_binding.binding = 0;
    vtx_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vtx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vtx_attr[0].binding = 0;
    vtx_attr[0].location = 0;
    vtx_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[0].offset = offsetof(struct owl_draw_cmd_vertex, pos);
    vtx_attr[1].binding = 0;
    vtx_attr[1].location = 1;
    vtx_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vtx_attr[2].binding = 0;
    vtx_attr[2].location = 2;
    vtx_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vtx_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vtx_attr);
    input.pVertexAttributeDescriptions = vtx_attr;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->swapchain_extent.width;
    viewport.height = renderer->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

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
    rasterization.cullMode = VK_CULL_MODE_NONE;
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
    multisample.rasterizationSamples = renderer->samples;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_att.blendEnable = VK_FALSE;
    color_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.colorBlendOp = VK_BLEND_OP_ADD;
    color_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.alphaBlendOp = VK_BLEND_OP_ADD;
    color_att.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_att;
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
    stages[0].module = renderer->basic_vert_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->basic_frag_shader;
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
    pipeline.layout = renderer->main_pipeline_layout;
    pipeline.renderPass = renderer->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_VK_CHECK(vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &renderer->pipelines[OWL_PIPELINE_TYPE_MAIN]));

    renderer->pipeline_layouts[OWL_PIPELINE_TYPE_MAIN] =
        renderer->main_pipeline_layout;
  }

  /* wires */
  {
    VkVertexInputBindingDescription vtx_binding;
    VkVertexInputAttributeDescription vtx_attr[3];
    VkPipelineVertexInputStateCreateInfo input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline;

    vtx_binding.binding = 0;
    vtx_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vtx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vtx_attr[0].binding = 0;
    vtx_attr[0].location = 0;
    vtx_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[0].offset = offsetof(struct owl_draw_cmd_vertex, pos);
    vtx_attr[1].binding = 0;
    vtx_attr[1].location = 1;
    vtx_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vtx_attr[2].binding = 0;
    vtx_attr[2].location = 2;
    vtx_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vtx_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vtx_attr);
    input.pVertexAttributeDescriptions = vtx_attr;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->swapchain_extent.width;
    viewport.height = renderer->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

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
    multisample.rasterizationSamples = renderer->samples;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_att.blendEnable = VK_FALSE;
    color_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.colorBlendOp = VK_BLEND_OP_ADD;
    color_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.alphaBlendOp = VK_BLEND_OP_ADD;
    color_att.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_att;
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
    stages[0].module = renderer->basic_vert_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->basic_frag_shader;
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
    pipeline.layout = renderer->main_pipeline_layout;
    pipeline.renderPass = renderer->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_VK_CHECK(vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &renderer->pipelines[OWL_PIPELINE_TYPE_WIRES]));

    renderer->pipeline_layouts[OWL_PIPELINE_TYPE_WIRES] =
        renderer->main_pipeline_layout;
  }

  /* font */
  {
    VkVertexInputBindingDescription vtx_binding;
    VkVertexInputAttributeDescription vtx_attr[3];
    VkPipelineVertexInputStateCreateInfo input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline;

    vtx_binding.binding = 0;
    vtx_binding.stride = sizeof(struct owl_draw_cmd_vertex);
    vtx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vtx_attr[0].binding = 0;
    vtx_attr[0].location = 0;
    vtx_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[0].offset = offsetof(struct owl_draw_cmd_vertex, pos);
    vtx_attr[1].binding = 0;
    vtx_attr[1].location = 1;
    vtx_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[1].offset = offsetof(struct owl_draw_cmd_vertex, color);
    vtx_attr[2].binding = 0;
    vtx_attr[2].location = 2;
    vtx_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[2].offset = offsetof(struct owl_draw_cmd_vertex, uv);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vtx_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vtx_attr);
    input.pVertexAttributeDescriptions = vtx_attr;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->swapchain_extent.width;
    viewport.height = renderer->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

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
    multisample.rasterizationSamples = renderer->samples;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_att.blendEnable = VK_TRUE;
    color_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_att.colorBlendOp = VK_BLEND_OP_ADD;
    color_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_att.alphaBlendOp = VK_BLEND_OP_ADD;
    color_att.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_att;
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
    stages[0].module = renderer->basic_vert_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->font_frag_shader;
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
    pipeline.layout = renderer->main_pipeline_layout;
    pipeline.renderPass = renderer->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_VK_CHECK(vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &renderer->pipelines[OWL_PIPELINE_TYPE_FONT]));

    renderer->pipeline_layouts[OWL_PIPELINE_TYPE_FONT] =
        renderer->main_pipeline_layout;
  }

  /* pbr */
  {
    VkVertexInputBindingDescription vtx_binding;
    VkVertexInputAttributeDescription vtx_attr[6];
    VkPipelineVertexInputStateCreateInfo input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline;

    vtx_binding.binding = 0;
    vtx_binding.stride = sizeof(struct owl_model_vertex);
    vtx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vtx_attr[0].binding = 0;
    vtx_attr[0].location = 0;
    vtx_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[0].offset = offsetof(struct owl_model_vertex, pos);
    vtx_attr[1].binding = 0;
    vtx_attr[1].location = 1;
    vtx_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[1].offset = offsetof(struct owl_model_vertex, normal);
    vtx_attr[2].binding = 0;
    vtx_attr[2].location = 2;
    vtx_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[2].offset = offsetof(struct owl_model_vertex, uv0);
    vtx_attr[3].binding = 0;
    vtx_attr[3].location = 3;
    vtx_attr[3].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[3].offset = offsetof(struct owl_model_vertex, uv1);
    vtx_attr[4].binding = 0;
    vtx_attr[4].location = 4;
    vtx_attr[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vtx_attr[4].offset = offsetof(struct owl_model_vertex, joint0);
    vtx_attr[5].binding = 0;
    vtx_attr[5].location = 5;
    vtx_attr[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vtx_attr[5].offset = offsetof(struct owl_model_vertex, weight0);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vtx_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vtx_attr);
    input.pVertexAttributeDescriptions = vtx_attr;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->swapchain_extent.width;
    viewport.height = renderer->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

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
    multisample.rasterizationSamples = renderer->samples;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_att.blendEnable = VK_FALSE;
    color_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.colorBlendOp = VK_BLEND_OP_ADD;
    color_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.alphaBlendOp = VK_BLEND_OP_ADD;
    color_att.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_att;
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
    stages[0].module = renderer->pbr_vert_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->pbr_frag_shader;
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
    pipeline.layout = renderer->pbr_pipeline_layout;
    pipeline.renderPass = renderer->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_VK_CHECK(vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &renderer->pipelines[OWL_PIPELINE_TYPE_PBR]));

    renderer->pipeline_layouts[OWL_PIPELINE_TYPE_PBR] =
        renderer->pbr_pipeline_layout;
  }

  /* pbr alpha blend */
  {
    VkVertexInputBindingDescription vtx_binding;
    VkVertexInputAttributeDescription vtx_attr[6];
    VkPipelineVertexInputStateCreateInfo input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_att;
    VkPipelineColorBlendStateCreateInfo color;
    VkPipelineDepthStencilStateCreateInfo depth;
    VkPipelineShaderStageCreateInfo stages[2];
    VkGraphicsPipelineCreateInfo pipeline;

    vtx_binding.binding = 0;
    vtx_binding.stride = sizeof(struct owl_model_vertex);
    vtx_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vtx_attr[0].binding = 0;
    vtx_attr[0].location = 0;
    vtx_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[0].offset = offsetof(struct owl_model_vertex, pos);
    vtx_attr[1].binding = 0;
    vtx_attr[1].location = 1;
    vtx_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vtx_attr[1].offset = offsetof(struct owl_model_vertex, normal);
    vtx_attr[2].binding = 0;
    vtx_attr[2].location = 2;
    vtx_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[2].offset = offsetof(struct owl_model_vertex, uv0);
    vtx_attr[3].binding = 0;
    vtx_attr[3].location = 3;
    vtx_attr[3].format = VK_FORMAT_R32G32_SFLOAT;
    vtx_attr[3].offset = offsetof(struct owl_model_vertex, uv1);
    vtx_attr[4].binding = 0;
    vtx_attr[4].location = 4;
    vtx_attr[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vtx_attr[4].offset = offsetof(struct owl_model_vertex, joint0);
    vtx_attr[5].binding = 0;
    vtx_attr[5].location = 5;
    vtx_attr[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vtx_attr[5].offset = offsetof(struct owl_model_vertex, weight0);

    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.pNext = NULL;
    input.flags = 0;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &vtx_binding;
    input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(vtx_attr);
    input.pVertexAttributeDescriptions = vtx_attr;

    input_assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.pNext = NULL;
    input_assembly.flags = 0;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.0F;
    viewport.y = 0.0F;
    viewport.width = renderer->swapchain_extent.width;
    viewport.height = renderer->swapchain_extent.height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderer->swapchain_extent;

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
    multisample.rasterizationSamples = renderer->samples;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0F;
    multisample.pSampleMask = NULL;
    multisample.alphaToCoverageEnable = VK_FALSE;
    multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                   \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                       \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

    color_att.blendEnable = VK_TRUE;
    color_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_att.colorBlendOp = VK_BLEND_OP_ADD;
    color_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_att.alphaBlendOp = VK_BLEND_OP_ADD;
    color_att.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

    color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color.pNext = NULL;
    color.flags = 0;
    color.logicOpEnable = VK_FALSE;
    color.logicOp = VK_LOGIC_OP_COPY;
    color.attachmentCount = 1;
    color.pAttachments = &color_att;
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
    stages[0].module = renderer->pbr_vert_shader;
    stages[0].pName = "main";
    stages[0].pSpecializationInfo = NULL;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].pNext = NULL;
    stages[1].flags = 0;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = renderer->pbr_frag_shader;
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
    pipeline.layout = renderer->pbr_pipeline_layout;
    pipeline.renderPass = renderer->main_render_pass;
    pipeline.subpass = 0;
    pipeline.basePipelineHandle = NULL;
    pipeline.basePipelineIndex = -1;

    OWL_VK_CHECK(vkCreateGraphicsPipelines(
        renderer->device, VK_NULL_HANDLE, 1, &pipeline, NULL,
        &renderer->pipelines[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND]));

    renderer->pipeline_layouts[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND] =
        renderer->pbr_pipeline_layout;
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_main_pipelines_(struct owl_vk_renderer *renderer) {
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_PIPELINE_TYPE_PBR_ALPHA_BLEND],
                    NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_PIPELINE_TYPE_PBR], NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_PIPELINE_TYPE_FONT], NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_PIPELINE_TYPE_WIRES], NULL);
  vkDestroyPipeline(renderer->device,
                    renderer->pipelines[OWL_PIPELINE_TYPE_MAIN], NULL);
}

OWL_INTERNAL enum owl_code owl_init_dyn_buf_(struct owl_vk_renderer *renderer,
                                             VkDeviceSize size) {
  enum owl_code code = OWL_SUCCESS;

  renderer->dyn_active_buf = 0;
  renderer->dyn_buf_size = size;

  /* init buffers */
  {
#define OWL_DOUBLE_BUFFER_USAGE                                                \
  (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |      \
   VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)

    owl_u32 i;
    VkBufferCreateInfo buf;
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.pNext = NULL;
    buf.flags = 0;
    buf.size = size;
    buf.usage = OWL_DOUBLE_BUFFER_USAGE;
    buf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf.queueFamilyIndexCount = 0;
    buf.pQueueFamilyIndices = NULL;

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_VK_CHECK(
          vkCreateBuffer(renderer->device, &buf, NULL, &renderer->dyn_bufs[i]));

#undef OWL_DOUBLE_BUFFER_USAGE
  }

  /* init memory */
  {
    VkMemoryRequirements req;
    VkMemoryAllocateInfo mem;

    vkGetBufferMemoryRequirements(renderer->device, renderer->dyn_bufs[0],
                                  &req);

    renderer->dyn_alignment = req.alignment;
    renderer->dyn_aligned_size = OWL_ALIGN(size, req.alignment);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize =
        renderer->dyn_aligned_size * OWL_RENDERER_DYNAMIC_BUFFER_COUNT;
    mem.memoryTypeIndex = owl_find_vk_mem_type(renderer, req.memoryTypeBits,
                                               OWL_VK_MEMORY_VIS_CPU_ONLY);

    OWL_VK_CHECK(
        vkAllocateMemory(renderer->device, &mem, NULL, &renderer->dyn_mem));
  }

  /* bind buffers to memory */
  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_VK_CHECK(vkBindBufferMemory(renderer->device, renderer->dyn_bufs[i],
                                      renderer->dyn_mem,
                                      i * renderer->dyn_aligned_size));
  }

  /* map memory */
  {
    owl_u32 i;
    void *data;
    OWL_VK_CHECK(vkMapMemory(renderer->device, renderer->dyn_mem, 0,
                             VK_WHOLE_SIZE, 0, &data));

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      renderer->dyn_data[i] = (owl_byte *)data + i * renderer->dyn_aligned_size;
  }

  /* init pvm descriptor sets */
  {
    owl_u32 i;
    VkDescriptorSetAllocateInfo sets;
    VkDescriptorSetLayout layouts[OWL_RENDERER_DYNAMIC_BUFFER_COUNT];

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      layouts[i] = renderer->pvm_set_layout;

    sets.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sets.pNext = NULL;
    sets.descriptorPool = renderer->set_pool;
    sets.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
    sets.pSetLayouts = layouts;

    OWL_VK_CHECK(vkAllocateDescriptorSets(renderer->device, &sets,
                                          renderer->dyn_pvm_sets));
  }

  /* write the pvm descriptor sets */
  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buf;

      buf.buffer = renderer->dyn_bufs[i];
      buf.offset = 0;
      buf.range = sizeof(struct owl_draw_cmd_ubo);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = renderer->dyn_pvm_sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buf;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device, 1, &write, 0, NULL);
    }
  }

  /* zero the offsets */
  {
    owl_u32 i;
    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      renderer->dyn_offsets[i] = 0;
  }

  return code;
}

OWL_INTERNAL void owl_deinit_dyn_buf_(struct owl_vk_renderer *renderer) {
  owl_u32 i;

  vkFreeDescriptorSets(renderer->device, renderer->set_pool,
                       OWL_ARRAY_SIZE(renderer->dyn_pvm_sets),
                       renderer->dyn_pvm_sets);

  vkFreeMemory(renderer->device, renderer->dyn_mem, NULL);

  for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyBuffer(renderer->device, renderer->dyn_bufs[i], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_dyn_garbage_(struct owl_vk_renderer *renderer) {
  renderer->dyn_garbage_bufs_count = 0;
  renderer->dyn_garbage_mems_count = 0;
  renderer->dyn_garbage_pvm_sets_count = 0;

  OWL_MEMSET(renderer->dyn_garbage_bufs, 0, sizeof(renderer->dyn_garbage_bufs));

  OWL_MEMSET(renderer->dyn_garbage_mems, 0, sizeof(renderer->dyn_garbage_mems));

  OWL_MEMSET(renderer->dyn_garbage_pvm_sets, 0,
             sizeof(renderer->dyn_garbage_pvm_sets));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_dyn_garbage_(struct owl_vk_renderer *renderer) {
  int i;

  for (i = 0; i < renderer->dyn_garbage_pvm_sets_count; ++i)
    vkFreeDescriptorSets(renderer->device, renderer->set_pool, 1,
                         &renderer->dyn_garbage_pvm_sets[i]);

  renderer->dyn_garbage_pvm_sets_count = 0;

  for (i = 0; i < renderer->dyn_garbage_mems_count; ++i)
    vkFreeMemory(renderer->device, renderer->dyn_garbage_mems[i], NULL);

  renderer->dyn_garbage_mems_count = 0;

  for (i = 0; i < renderer->dyn_garbage_bufs_count; ++i)
    vkDestroyBuffer(renderer->device, renderer->dyn_garbage_bufs[i], NULL);

  renderer->dyn_garbage_bufs_count = 0;
}

OWL_INTERNAL enum owl_code
owl_move_dyn_to_garbage_(struct owl_vk_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  {
    int i;
    /* previous count */
    int prev_count = renderer->dyn_garbage_bufs_count;
    /* new count */
    int new_count =
        renderer->dyn_garbage_bufs_count + OWL_RENDERER_DYNAMIC_BUFFER_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      renderer->dyn_garbage_bufs[prev_count + i] = renderer->dyn_bufs[i];

    renderer->dyn_garbage_bufs_count = new_count;
  }

  {
    int i;
    /* previous count */
    int prev_count = renderer->dyn_garbage_pvm_sets_count;
    /* new count */
    int new_count = renderer->dyn_garbage_pvm_sets_count +
                    OWL_RENDERER_DYNAMIC_BUFFER_COUNT;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    for (i = 0; i < OWL_RENDERER_DYNAMIC_BUFFER_COUNT; ++i)
      renderer->dyn_garbage_pvm_sets[prev_count + i] =
          renderer->dyn_pvm_sets[i];

    renderer->dyn_garbage_pvm_sets_count = new_count;
  }

  {
    /* previous count */
    int prev_count = renderer->dyn_garbage_mems_count;
    /* new count */
    int new_count = renderer->dyn_garbage_mems_count + 1;

    if (OWL_RENDERER_MAX_GARBAGE_ITEMS <= new_count) {
      code = OWL_ERROR_OUT_OF_BOUNDS;
      goto end;
    }

    renderer->dyn_garbage_mems[prev_count + 0] = renderer->dyn_mem;
    renderer->dyn_garbage_mems_count = new_count;
  }

end:
  return code;
}

enum owl_code owl_init_vk_renderer(struct owl_vk_plataform const *plataform,
                                   struct owl_vk_renderer *renderer) {
  enum owl_code code;

  renderer->width = plataform->framebuffer_width;
  renderer->height = plataform->framebuffer_height;

  if (OWL_SUCCESS != (code = owl_init_instance_(plataform, renderer)))
    goto end;

#ifndef NDEBUG
  if (OWL_SUCCESS != (code = owl_init_debug_(renderer)))
    goto end;
#endif

  if (OWL_SUCCESS != (code = owl_init_surface_(plataform, renderer)))
    goto end_deinit_instance;

  if (OWL_SUCCESS != (code = owl_init_device_(renderer)))
    goto end_deinit_surface;

  if (OWL_SUCCESS != (code = owl_init_swapchain_(plataform, renderer)))
    goto end_deinit_device;

  if (OWL_SUCCESS != (code = owl_init_general_pools_(renderer)))
    goto end_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_init_frame_cmds_(renderer)))
    goto end_deinit_general_pools;

  if (OWL_SUCCESS != (code = owl_init_frame_sync_(renderer)))
    goto end_deinit_frame_cmds;

  if (OWL_SUCCESS != (code = owl_init_main_render_pass_(renderer)))
    goto end_deinit_frame_sync;

  if (OWL_SUCCESS != (code = owl_init_main_attachments_(renderer)))
    goto end_deinit_main_render_pass;

  if (OWL_SUCCESS != (code = owl_init_main_framebuffers_(renderer)))
    goto end_deinit_main_attachments;

  if (OWL_SUCCESS != (code = owl_init_main_set_layouts_(renderer)))
    goto end_deinit_main_framebuffers;

  if (OWL_SUCCESS != (code = owl_init_main_pipeline_layouts_(renderer)))
    goto end_deinit_main_set_layouts;

  if (OWL_SUCCESS != (code = owl_init_main_shaders_(renderer)))
    goto end_deinit_main_pipeline_layout;

  if (OWL_SUCCESS != (code = owl_init_main_pipelines_(renderer)))
    goto end_deinit_main_shaders;

  if (OWL_SUCCESS != (code = owl_init_dyn_buf_(renderer, 1024 * 1024)))
    goto end_deinit_main_pipelines;

  if (OWL_SUCCESS != (code = owl_init_dyn_garbage_(renderer)))
    goto end_deinit_dyn_buf;

  goto end;

end_deinit_dyn_buf:
  owl_deinit_dyn_buf_(renderer);

end_deinit_main_pipelines:
  owl_deinit_main_pipelines_(renderer);

end_deinit_main_shaders:
  owl_deinit_main_shaders_(renderer);

end_deinit_main_pipeline_layout:
  owl_deinit_main_pipeline_layouts_(renderer);

end_deinit_main_set_layouts:
  owl_deinit_main_set_layouts_(renderer);

end_deinit_main_framebuffers:
  owl_deinit_main_framebuffers_(renderer);

end_deinit_main_attachments:
  owl_deinit_main_attachments_(renderer);

end_deinit_main_render_pass:
  owl_deinit_main_render_pass_(renderer);

end_deinit_frame_sync:
  owl_deinit_frame_sync_(renderer);

end_deinit_frame_cmds:
  owl_deinit_frame_cmds_(renderer);

end_deinit_general_pools:
  owl_deinit_general_pools_(renderer);

end_deinit_swapchain:
  owl_deinit_swapchain_(renderer);

end_deinit_device:
  owl_deinit_device_(renderer);

end_deinit_surface:
  owl_deinit_surface_(renderer);

end_deinit_instance:
  owl_deinit_instance_(renderer);

end:
  return code;
}

void owl_deinit_vk_renderer(struct owl_vk_renderer *renderer) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  owl_deinit_dyn_garbage_(renderer);
  owl_deinit_dyn_buf_(renderer);
  owl_deinit_main_pipelines_(renderer);
  owl_deinit_main_shaders_(renderer);
  owl_deinit_main_pipeline_layouts_(renderer);
  owl_deinit_main_set_layouts_(renderer);
  owl_deinit_main_framebuffers_(renderer);
  owl_deinit_main_attachments_(renderer);
  owl_deinit_main_render_pass_(renderer);
  owl_deinit_frame_sync_(renderer);
  owl_deinit_frame_cmds_(renderer);
  owl_deinit_general_pools_(renderer);
  owl_deinit_swapchain_(renderer);
  owl_deinit_device_(renderer);
  owl_deinit_surface_(renderer);

#ifndef NDEBUG
  owl_deinit_debug_(renderer);
#endif

  owl_deinit_instance_(renderer);
}

enum owl_code owl_reserve_dyn_buf_mem(struct owl_vk_renderer *renderer,
                                      VkDeviceSize size) {
  enum owl_code code = OWL_SUCCESS;
  int const active = renderer->dyn_active_buf;
  VkDeviceSize required = renderer->dyn_offsets[active] + size;

  if (required > renderer->dyn_buf_size) {
    vkUnmapMemory(renderer->device, renderer->dyn_mem);

    if (OWL_SUCCESS != (code = owl_move_dyn_to_garbage_(renderer)))
      goto end;

    if (OWL_SUCCESS != (code = owl_init_dyn_buf_(renderer, 2 * required)))
      goto end;
  }

end:
  return code;
}

void owl_clear_dyn_garbage(struct owl_vk_renderer *renderer) {
  owl_deinit_dyn_garbage_(renderer);
}

enum owl_code owl_reinit_vk_swapchain(struct owl_vk_plataform const *plataform,
                                      struct owl_vk_renderer *renderer) {
  enum owl_code code = OWL_SUCCESS;

  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device));

  owl_deinit_main_pipelines_(renderer);
  owl_deinit_main_framebuffers_(renderer);
  owl_deinit_main_attachments_(renderer);
  owl_deinit_main_render_pass_(renderer);
  owl_deinit_frame_sync_(renderer);
  owl_deinit_swapchain_(renderer);

  if (OWL_SUCCESS != (code = owl_init_swapchain_(plataform, renderer)))
    goto end;

  if (OWL_SUCCESS != (code = owl_init_frame_sync_(renderer)))
    goto end_deinit_swapchain;

  if (OWL_SUCCESS != (code = owl_init_main_render_pass_(renderer)))
    goto end_deinit_frame_sync;

  if (OWL_SUCCESS != (code = owl_init_main_attachments_(renderer)))
    goto end_deinit_main_render_pass;

  if (OWL_SUCCESS != (code = owl_init_main_framebuffers_(renderer)))
    goto end_deinit_main_attachments;

  if (OWL_SUCCESS != (code = owl_init_main_pipelines_(renderer)))
    goto end_deinit_main_framebuffers;

  goto end;

end_deinit_main_framebuffers:
  owl_deinit_main_framebuffers_(renderer);

end_deinit_main_attachments:
  owl_deinit_main_attachments_(renderer);

end_deinit_main_render_pass:
  owl_deinit_main_render_pass_(renderer);

end_deinit_frame_sync:
  owl_deinit_frame_sync_(renderer);

end_deinit_swapchain:
  owl_deinit_swapchain_(renderer);

end:
  return code;
}

int owl_is_dyn_buf_flushed(struct owl_vk_renderer *renderer) {
  return 0 == renderer->dyn_offsets[renderer->dyn_active_buf];
}

void owl_flush_dyn_buf(struct owl_vk_renderer *renderer) {
  renderer->dyn_offsets[renderer->dyn_active_buf] = 0;
}

void owl_clear_dyn_buf_garbage(struct owl_vk_renderer *renderer) {
  owl_deinit_dyn_garbage_(renderer);
}

enum owl_code owl_create_renderer(struct owl_window *window,
                                  struct owl_vk_renderer **renderer) {
  struct owl_vk_plataform plataform;
  enum owl_code code = OWL_SUCCESS;

  if (!(*renderer = OWL_MALLOC(sizeof(**renderer))))
    return OWL_ERROR_BAD_ALLOC;

  owl_fill_vk_plataform(window, &plataform);
  code = owl_init_vk_renderer(&plataform, *renderer);

  if (OWL_SUCCESS != code)
    OWL_FREE(*renderer);

  return code;
}

enum owl_code owl_recreate_swapchain(struct owl_window *window,
                                     struct owl_vk_renderer *renderer) {
  struct owl_vk_plataform plataform;
  owl_fill_vk_plataform(window, &plataform);
  return owl_reinit_vk_swapchain(&plataform, renderer);
}

void owl_destroy_renderer(struct owl_vk_renderer *renderer) {
  owl_deinit_vk_renderer(renderer);
  OWL_FREE(renderer);
}

void *owl_dyn_buf_alloc(struct owl_vk_renderer *renderer, VkDeviceSize size,
                        struct owl_dyn_buf_ref *ref) {
  owl_byte *data;
  int const active = renderer->dyn_active_buf;

  if (OWL_SUCCESS != owl_reserve_dyn_buf_mem(renderer, size))
    return NULL;

  ref->offset32 = (owl_u32)renderer->dyn_offsets[active];
  ref->offset = renderer->dyn_offsets[active];
  ref->buf = renderer->dyn_bufs[active];
  ref->pvm_set = renderer->dyn_pvm_sets[active];

  data = renderer->dyn_data[active] + renderer->dyn_offsets[active];

  renderer->dyn_offsets[active] =
      OWL_ALIGN(renderer->dyn_offsets[active] + size, renderer->dyn_alignment);

  return data;
}

OWL_INTERNAL VkMemoryPropertyFlags
owl_vk_as_property_flags(enum owl_vk_mem_vis vis) {
  switch (vis) {
  case OWL_VK_MEMORY_VIS_CPU_ONLY:
  case OWL_VK_MEMORY_VIS_CPU_TO_GPU:
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  case OWL_VK_MEMORY_VIS_GPU_ONLY:
    return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
}

owl_u32 owl_find_vk_mem_type(struct owl_vk_renderer const *renderer,
                             owl_u32 filter, enum owl_vk_mem_vis vis) {
  owl_u32 i;
  VkMemoryPropertyFlags props = owl_vk_as_property_flags(vis);

  for (i = 0; i < renderer->device_mem_properties.memoryTypeCount; ++i) {
    owl_u32 f = renderer->device_mem_properties.memoryTypes[i].propertyFlags;

    if ((f & props) && (filter & (1U << i)))
      return i;
  }

  return OWL_VK_MEMORY_TYPE_NONE;
}

enum owl_code owl_bind_pipeline(struct owl_vk_renderer *renderer,
                                enum owl_pipeline_type type) {
  int const active = renderer->dyn_active_buf;

  renderer->bound_pipeline = type;

  if (OWL_PIPELINE_TYPE_NONE == type)
    return OWL_SUCCESS;

  vkCmdBindPipeline(renderer->frame_cmd_bufs[active],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipelines[type]);

  return OWL_SUCCESS;
}

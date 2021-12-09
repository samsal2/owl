#include <owl/internal.h>
#include <owl/render_group.h>
#include <owl/renderer.inl>
#include <owl/types.h>
#include <owl/vkutil.h>
#include <owl/window.h>
#include <owl/window.inl>

#define OWL_VK_GET_INSTANCE_PROC_ADDR(i, fn)                                 \
  ((PFN_##fn)vkGetInstanceProcAddr((i), #fn))

#define OWL_SURFACE_PROVIDER_EXECUTE(renderer, provider, out)                \
  (provider)->get(renderer, (provider)->data, out)

#ifdef OWL_ENABLE_VALIDATION
OWL_GLOBAL char const *const validation_layers[] = {
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
owl_init_instance_(struct owl_vk_extensions const *extensions,
                   struct owl_renderer *renderer) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = "OTTER";
  app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION(1, 0, 0);

  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pNext = NULL;
  instance.flags = 0;
  instance.pApplicationInfo = &app;
  instance.enabledLayerCount = OWL_ARRAY_SIZE(validation_layers);
  instance.ppEnabledLayerNames = validation_layers;
  instance.enabledExtensionCount = (OwlU32)extensions->count;
  instance.ppEnabledExtensionNames = extensions->names;

  OWL_VK_CHECK(vkCreateInstance(&instance, NULL, &renderer->instance));

  return OWL_SUCCESS;
}

#define OWL_DEBUG_SEVERITY_FLAGS                                             \
  (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |                         \
   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |                         \
   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)

#define OWL_DEBUG_TYPE_FLAGS                                                 \
  (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |                             \
   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |                          \
   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)

OWL_INTERNAL enum owl_code owl_init_dbg_(struct owl_renderer *renderer) {
  VkDebugUtilsMessengerCreateInfoEXT debug;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;

  vkCreateDebugUtilsMessengerEXT = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkCreateDebugUtilsMessengerEXT);

  OWL_ASSERT(vkCreateDebugUtilsMessengerEXT);

  debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug.pNext = NULL;
  debug.flags = 0;
  debug.messageSeverity = OWL_DEBUG_SEVERITY_FLAGS;
  debug.messageType = OWL_DEBUG_TYPE_FLAGS;
  debug.pfnUserCallback = owl_vk_debug_cb_;
  debug.pUserData = NULL;

  OWL_VK_CHECK(vkCreateDebugUtilsMessengerEXT(renderer->instance, &debug,
                                              NULL, &renderer->debug));
  return OWL_SUCCESS;
}

#undef OWL_DEBUG_TYPE_FLAGS
#undef OWL_DEBUG_SEVERITY_FLAGS

OWL_INTERNAL void owl_deinit_dbg_(struct owl_renderer *renderer) {
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger;

  vkDestroyDebugUtilsMessenger = OWL_VK_GET_INSTANCE_PROC_ADDR(
      renderer->instance, vkDestroyDebugUtilsMessengerEXT);

  OWL_ASSERT(vkDestroyDebugUtilsMessenger);

  vkDestroyDebugUtilsMessenger(renderer->instance, renderer->debug, NULL);
}

#else
OWL_INTERNAL enum owl_code
owl_init_instance_(struct owl_vk_extensions const *extensions,
                   struct owl_renderer *renderer) {
  VkApplicationInfo app;
  VkInstanceCreateInfo instance;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = "OTTER";
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
  instance.enabledExtensionCount = (OwlU32)extensions->count;
  instance.ppEnabledExtensionNames = extensions->names;

  OWL_VK_CHECK(vkCreateInstance(&instance, NULL, &renderer->instance));

  return OWL_SUCCESS;
}
#endif
OWL_INTERNAL void owl_deinit_instance_(struct owl_renderer *renderer) {
  vkDestroyInstance(renderer->instance, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_surface_(struct owl_vk_surface_provider const *provider,
                  struct owl_renderer *renderer) {
  return OWL_SURFACE_PROVIDER_EXECUTE(renderer, provider, &renderer->surface);
}

OWL_INTERNAL void owl_deinit_surface_(struct owl_renderer *renderer) {
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
}

OWL_INTERNAL enum owl_code owl_init_device_(struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;

  /* fill options */
  {
    OwlU32 count;
    OWL_VK_CHECK(
        vkEnumeratePhysicalDevices(renderer->instance, &count, NULL));

    OWL_ASSERT(OWL_MAX_PHYSICAL_DEVICE_OPTIONS > count);

    OWL_VK_CHECK(vkEnumeratePhysicalDevices(renderer->instance, &count,
                                            renderer->device.options));

    renderer->device.options_count = (int)count;
  }

  /* select physical device and queue families */
  {
#define OWL_MAX_EXTENSIONS 64
    int i;
    static VkExtensionProperties extensions[OWL_MAX_EXTENSIONS];

    for (i = 0; i < renderer->device.options_count; ++i) {
      OwlU32 formats, modes, count;

      OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
          renderer->device.options[i], renderer->surface, &formats, NULL));

      if (!formats)
        continue;

      OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
          renderer->device.options[i], renderer->surface, &modes, NULL));

      if (!modes)
        continue;

      if (!owl_vk_query_families(renderer, renderer->device.options[i],
                                 renderer->device.families))
        continue;

      vkGetPhysicalDeviceFeatures(renderer->device.options[i],
                                  &renderer->device.features);

      if (!renderer->device.features.samplerAnisotropy)
        continue;

      OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
          renderer->device.options[i], NULL, &count, NULL));

      OWL_ASSERT(OWL_MAX_EXTENSIONS > count);

      OWL_VK_CHECK(vkEnumerateDeviceExtensionProperties(
          renderer->device.options[i], NULL, &count, extensions));

      if (!owl_vk_validate_extensions(count, extensions))
        continue;

      renderer->device.physical = renderer->device.options[i];

      OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          renderer->device.physical, renderer->surface,
          &renderer->support.capabilities));

      vkGetPhysicalDeviceProperties(renderer->device.physical,
                                    &renderer->device.props);

      vkGetPhysicalDeviceMemoryProperties(renderer->device.physical,
                                          &renderer->device.mem_props);

      break;
    }

    OWL_ASSERT(renderer->device.options_count != i);

#undef OWL_MAX_EXTENSIONS
  }

  /* select surface format */
  {
#define OWL_MAX_SURFACE_FORMATS 16
    OwlU32 i, count;
    VkSurfaceFormatKHR formats[OWL_MAX_SURFACE_FORMATS];

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        renderer->device.physical, renderer->surface, &count, NULL));

    OWL_ASSERT(OWL_MAX_SURFACE_FORMATS > count);

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        renderer->device.physical, renderer->surface, &count, formats));

    for (i = 0; i < count; ++i) {
      if (VK_FORMAT_B8G8R8A8_SRGB != formats[i].format)
        continue;

      if (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR != formats[i].colorSpace)
        continue;

      renderer->support.format.format = VK_FORMAT_B8G8R8A8_SRGB;
      renderer->support.format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      break;
    }

    OWL_ASSERT(OWL_MAX_SURFACE_FORMATS != i);

#undef OWL_MAX_SURFACE_FORMATS
  }

  /* find multisampling option */
  {
    VkSampleCountFlags const samples =
        renderer->device.props.limits.framebufferColorSampleCounts &
        renderer->device.props.limits.framebufferDepthSampleCounts;

    if (VK_SAMPLE_COUNT_2_BIT & samples) {
      renderer->support.samples = VK_SAMPLE_COUNT_2_BIT;
    } else {
      OWL_ASSERT(0 && "disabling multisampling is not supported");
      renderer->support.samples = VK_SAMPLE_COUNT_1_BIT;
    }
  }

  {
    VkDeviceCreateInfo device;
    VkDeviceQueueCreateInfo queues[2];
    float const prio = 1.0F;
    char const *extensions[] = OWL_VK_DEVICE_EXTENSIONS;

    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].pNext = NULL;
    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].flags = 0;
    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].queueFamilyIndex =
        renderer->device.families[OWL_VK_QUEUE_TYPE_GRAPHICS];
    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].queueCount = 1;
    queues[OWL_VK_QUEUE_TYPE_GRAPHICS].pQueuePriorities = &prio;

    queues[OWL_VK_QUEUE_TYPE_PRESENT].sType =
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[OWL_VK_QUEUE_TYPE_PRESENT].pNext = NULL;
    queues[OWL_VK_QUEUE_TYPE_PRESENT].flags = 0;
    queues[OWL_VK_QUEUE_TYPE_PRESENT].queueFamilyIndex =
        renderer->device.families[OWL_VK_QUEUE_TYPE_PRESENT];
    queues[OWL_VK_QUEUE_TYPE_PRESENT].queueCount = 1;
    queues[OWL_VK_QUEUE_TYPE_PRESENT].pQueuePriorities = &prio;

    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = NULL;
    device.flags = 0;
    device.queueCreateInfoCount = (OwlU32)owl_vk_queue_count(renderer);
    device.pQueueCreateInfos = queues;
    device.enabledLayerCount = 0;      /* deprecated */
    device.ppEnabledLayerNames = NULL; /* deprecated */
    device.enabledExtensionCount = (OwlU32)OWL_ARRAY_SIZE(extensions);
    device.ppEnabledExtensionNames = extensions;
    device.pEnabledFeatures = &renderer->device.features;

    OWL_VK_CHECK(vkCreateDevice(renderer->device.physical, &device, NULL,
                                &renderer->device.logical));
  }

  vkGetDeviceQueue(renderer->device.logical,
                   renderer->device.families[OWL_VK_QUEUE_TYPE_GRAPHICS], 0,
                   &renderer->device.queues[OWL_VK_QUEUE_TYPE_GRAPHICS]);

  vkGetDeviceQueue(renderer->device.logical,
                   renderer->device.families[OWL_VK_QUEUE_TYPE_PRESENT], 0,
                   &renderer->device.queues[OWL_VK_QUEUE_TYPE_PRESENT]);
  return err;
}

OWL_INTERNAL void owl_deinit_device_(struct owl_renderer *renderer) {
  vkDestroyDevice(renderer->device.logical, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_pass_(struct owl_renderer *renderer) {
  VkAttachmentReference attach[3];
  VkAttachmentDescription desc[3];
  VkSubpassDescription subpass_desc;
  VkSubpassDependency subpass_dep;
  VkRenderPassCreateInfo pass;

  attach[0].attachment = 0;
  attach[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  attach[1].attachment = 1;
  attach[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  attach[2].attachment = 2;
  attach[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  desc[0].flags = 0;
  desc[0].format = renderer->support.format.format;
  desc[0].samples = renderer->support.samples;
  desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  desc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  desc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  desc[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  desc[1].flags = 0;
  desc[1].format = VK_FORMAT_D32_SFLOAT;
  desc[1].samples = renderer->support.samples;
  desc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  desc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  desc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  desc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  desc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  desc[2].flags = 0;
  desc[2].format = renderer->support.format.format;
  desc[2].samples = VK_SAMPLE_COUNT_1_BIT;
  desc[2].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  desc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  desc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  desc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  desc[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  subpass_desc.flags = 0;
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_desc.inputAttachmentCount = 0;
  subpass_desc.pInputAttachments = NULL;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &attach[0];
  subpass_desc.pResolveAttachments = &attach[2];
  subpass_desc.pDepthStencilAttachment = &attach[1];
  subpass_desc.preserveAttachmentCount = 0;
  subpass_desc.pPreserveAttachments = NULL;

#define OWL_SRC_STAGE                                                        \
  (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                           \
   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)

#define OWL_DST_STAGE                                                        \
  (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |                           \
   VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)

#define OWL_DST_ACCESS                                                       \
  (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |                                    \
   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

  subpass_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dep.dstSubpass = 0;
  subpass_dep.srcStageMask = OWL_SRC_STAGE;
  subpass_dep.srcAccessMask = 0;
  subpass_dep.dstStageMask = OWL_DST_STAGE;
  subpass_dep.dstAccessMask = OWL_DST_ACCESS;
  subpass_dep.dependencyFlags = 0;

#undef OWL_DST_ACCESS
#undef OWL_DST_STAGE
#undef OTTET_SRC_STAGE

  pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  pass.pNext = NULL;
  pass.flags = 0;
  pass.attachmentCount = OWL_ARRAY_SIZE(desc);
  pass.pAttachments = desc;
  pass.subpassCount = 1;
  pass.pSubpasses = &subpass_desc;
  pass.dependencyCount = 1;
  pass.pDependencies = &subpass_dep;

  OWL_VK_CHECK(vkCreateRenderPass(renderer->device.logical, &pass, NULL,
                                  &renderer->main_pass));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_main_pass_(struct owl_renderer *renderer) {
  vkDestroyRenderPass(renderer->device.logical, renderer->main_pass, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_color_attach_(struct owl_extent const *extent,
                       struct owl_renderer *renderer) {
  OtterVkMemoryType type;
  VkImageCreateInfo img;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo mem;
  VkImageViewCreateInfo view;
  enum owl_code err = OWL_SUCCESS;

  img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img.pNext = NULL;
  img.flags = 0;
  img.imageType = VK_IMAGE_TYPE_2D;
  img.format = renderer->support.format.format;
  img.extent.width = (OwlU32)extent->width;
  img.extent.height = (OwlU32)extent->height;
  img.extent.depth = 1;
  img.mipLevels = 1;
  img.arrayLayers = 1;
  img.samples = renderer->support.samples;
  img.tiling = VK_IMAGE_TILING_OPTIMAL;
  img.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  img.queueFamilyIndexCount = 0;
  img.pQueueFamilyIndices = NULL;
  img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  OWL_VK_CHECK(vkCreateImage(renderer->device.logical, &img, NULL,
                             &renderer->color_attach.img));

  vkGetImageMemoryRequirements(renderer->device.logical,
                               renderer->color_attach.img, &req);

  if (OWL_VK_MEMORY_TYPE_NONE ==
      (type = owl_vk_find_mem_type(renderer, req.memoryTypeBits,
                                   OWL_VK_MEMORY_VISIBILITY_GPU_ONLY)))
    goto end;

  mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem.pNext = NULL;
  mem.allocationSize = req.size;
  mem.memoryTypeIndex = type;

  OWL_VK_CHECK(vkAllocateMemory(renderer->device.logical, &mem, NULL,
                                &renderer->color_attach.mem));

  OWL_VK_CHECK(vkBindImageMemory(renderer->device.logical,
                                 renderer->color_attach.img,
                                 renderer->color_attach.mem, 0));

  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.pNext = NULL;
  view.flags = 0;
  view.image = renderer->color_attach.img;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.format = renderer->support.format.format;
  view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  view.subresourceRange.baseMipLevel = 0;
  view.subresourceRange.levelCount = 1;
  view.subresourceRange.baseArrayLayer = 0;
  view.subresourceRange.layerCount = 1;

  OWL_VK_CHECK(vkCreateImageView(renderer->device.logical, &view, NULL,
                                 &renderer->color_attach.view));

end:
  return err;
}

OWL_INTERNAL void owl_deinit_color_attach_(struct owl_renderer *renderer) {
  vkDestroyImageView(renderer->device.logical, renderer->color_attach.view,
                     NULL);
  vkFreeMemory(renderer->device.logical, renderer->color_attach.mem, NULL);
  vkDestroyImage(renderer->device.logical, renderer->color_attach.img, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_depth_attach_(struct owl_extent const *extent,
                       struct owl_renderer *renderer) {
  OtterVkMemoryType type;
  VkImageCreateInfo img;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo mem;
  VkImageViewCreateInfo view;
  enum owl_code err = OWL_SUCCESS;

  img.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  img.pNext = NULL;
  img.flags = 0;
  img.imageType = VK_IMAGE_TYPE_2D;
  img.format = VK_FORMAT_D32_SFLOAT;
  img.extent.width = (OwlU32)extent->width;
  img.extent.height = (OwlU32)extent->height;
  img.extent.depth = 1;
  img.mipLevels = 1;
  img.arrayLayers = 1;
  img.samples = renderer->support.samples;
  img.tiling = VK_IMAGE_TILING_OPTIMAL;
  img.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  img.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  img.queueFamilyIndexCount = 0;
  img.pQueueFamilyIndices = NULL;
  img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  OWL_VK_CHECK(vkCreateImage(renderer->device.logical, &img, NULL,
                             &renderer->depth_attach.img));

  vkGetImageMemoryRequirements(renderer->device.logical,
                               renderer->depth_attach.img, &req);

  if (OWL_VK_MEMORY_TYPE_NONE ==
      (type = owl_vk_find_mem_type(renderer, req.memoryTypeBits,
                                   OWL_VK_MEMORY_VISIBILITY_GPU_ONLY)))
    goto end;

  mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem.pNext = NULL;
  mem.allocationSize = req.size;
  mem.memoryTypeIndex = type;

  OWL_VK_CHECK(vkAllocateMemory(renderer->device.logical, &mem, NULL,
                                &renderer->depth_attach.mem));

  OWL_VK_CHECK(vkBindImageMemory(renderer->device.logical,
                                 renderer->depth_attach.img,
                                 renderer->depth_attach.mem, 0));

  view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view.pNext = NULL;
  view.flags = 0;
  view.image = renderer->depth_attach.img;
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

  OWL_VK_CHECK(vkCreateImageView(renderer->device.logical, &view, NULL,
                                 &renderer->depth_attach.view));

end:
  return err;
}

OWL_INTERNAL void owl_deinit_depth_attach_(struct owl_renderer *renderer) {
  vkDestroyImageView(renderer->device.logical, renderer->depth_attach.view,
                     NULL);
  vkFreeMemory(renderer->device.logical, renderer->depth_attach.mem, NULL);
  vkDestroyImage(renderer->device.logical, renderer->depth_attach.img, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_swapchain_(struct owl_extent const *extent,
                    struct owl_renderer *renderer) {
  VkPresentModeKHR mode;

  /* select the swapchain extent */
  {
#define OWL_VK_NO_RESTRICTIONS (OwlU32) - 1
    if (OWL_VK_NO_RESTRICTIONS ==
        renderer->support.capabilities.currentExtent.width) {
      renderer->swapchain.extent =
          renderer->support.capabilities.currentExtent;
    } else {
      OwlU32 min_width = renderer->support.capabilities.minImageExtent.width;
      OwlU32 min_height =
          renderer->support.capabilities.minImageExtent.height;
      OwlU32 max_width = renderer->support.capabilities.maxImageExtent.width;
      OwlU32 max_height =
          renderer->support.capabilities.maxImageExtent.height;

      renderer->swapchain.extent.width =
          OWL_CLAMP((OwlU32)extent->width, min_width, max_width);
      renderer->swapchain.extent.height =
          OWL_CLAMP((OwlU32)extent->height, min_height, max_height);
    }
#undef OWL_VK_NO_RESTRICTIONS
  }

  /* find present mode */
  {
#define OWL_MAX_PRESENT_MODES 6
    OwlU32 i, count;
    VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        renderer->device.physical, renderer->surface, &count, NULL));

    OWL_ASSERT(OWL_MAX_PRESENT_MODES > count);

    OWL_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        renderer->device.physical, renderer->surface, &count, modes));

    mode = modes[0]; /* FIFO is guaranteed, so count should always be atleast
                        1, else we go to UB land */
    for (i = 0; i < count; ++i)
      if (VK_PRESENT_MODE_MAILBOX_KHR == (mode = modes[count - i - 1]))
        break;

#undef OWL_MAX_PRESENT_MODES
  }

  {
    VkSwapchainCreateInfoKHR swapchain;
    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.pNext = NULL;
    swapchain.flags = 0;
    swapchain.surface = renderer->surface;
    swapchain.minImageCount = 2;
    swapchain.imageFormat = renderer->support.format.format;
    swapchain.imageColorSpace = renderer->support.format.colorSpace;
    swapchain.imageExtent.width = renderer->swapchain.extent.width;
    swapchain.imageExtent.height = renderer->swapchain.extent.height;
    swapchain.imageArrayLayers = 1;
    swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain.preTransform = renderer->support.capabilities.currentTransform;
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain.presentMode = mode;
    swapchain.clipped = VK_TRUE;
    swapchain.oldSwapchain = NULL;

    if (owl_vk_shares_families(renderer)) {
      swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain.queueFamilyIndexCount = 0;
      swapchain.pQueueFamilyIndices = NULL;
    } else {
      swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      swapchain.queueFamilyIndexCount = OWL_VK_QUEUE_TYPE_COUNT;
      swapchain.pQueueFamilyIndices = renderer->device.families;
    }

    OWL_VK_CHECK(vkCreateSwapchainKHR(renderer->device.logical, &swapchain,
                                      NULL, &renderer->swapchain.handle));
  }

  OWL_VK_CHECK(vkGetSwapchainImagesKHR(renderer->device.logical,
                                       renderer->swapchain.handle,
                                       &renderer->swapchain.img_count, NULL));

  OWL_ASSERT(OWL_MAX_SWAPCHAIN_IMAGES > renderer->swapchain.img_count);

  OWL_VK_CHECK(vkGetSwapchainImagesKHR(
      renderer->device.logical, renderer->swapchain.handle,
      &renderer->swapchain.img_count, renderer->swapchain.imgs));
  {
    OwlU32 i;
    for (i = 0; i < renderer->swapchain.img_count; ++i) {
      VkImageViewCreateInfo view;

      view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view.pNext = NULL;
      view.flags = 0;
      view.image = renderer->swapchain.imgs[i];
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view.format = renderer->support.format.format;
      view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.baseMipLevel = 0;
      view.subresourceRange.levelCount = 1;
      view.subresourceRange.baseArrayLayer = 0;
      view.subresourceRange.layerCount = 1;

      OWL_VK_CHECK(vkCreateImageView(renderer->device.logical, &view, NULL,
                                     &renderer->swapchain.views[i]));
    }
  }
  {
    OwlU32 i;
    for (i = 0; i < renderer->swapchain.img_count; ++i) {
      VkImageView attach[3];
      VkFramebufferCreateInfo framebuffer;

      attach[0] = renderer->color_attach.view;
      attach[1] = renderer->depth_attach.view;
      attach[2] = renderer->swapchain.views[i];

      framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer.pNext = NULL;
      framebuffer.flags = 0;
      framebuffer.renderPass = renderer->main_pass;
      framebuffer.attachmentCount = OWL_ARRAY_SIZE(attach);
      framebuffer.pAttachments = attach;
      framebuffer.width = renderer->swapchain.extent.width;
      framebuffer.height = renderer->swapchain.extent.height;
      framebuffer.layers = 1;

      OWL_VK_CHECK(vkCreateFramebuffer(renderer->device.logical, &framebuffer,
                                       NULL,
                                       &renderer->swapchain.framebuffers[i]));
    }
  }

  renderer->swapchain.clear_vals[0].color.float32[0] = 0.0F;
  renderer->swapchain.clear_vals[0].color.float32[1] = 0.0F;
  renderer->swapchain.clear_vals[0].color.float32[2] = 0.0F;
  renderer->swapchain.clear_vals[0].color.float32[3] = 1.0F;
  renderer->swapchain.clear_vals[1].depthStencil.depth = 1.0F;
  renderer->swapchain.clear_vals[1].depthStencil.stencil = 0.0F;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_swapchain_(struct owl_renderer *renderer) {
  OwlU32 i;

  for (i = 0; i < renderer->swapchain.img_count; ++i)
    vkDestroyFramebuffer(renderer->device.logical,
                         renderer->swapchain.framebuffers[i], NULL);

  for (i = 0; i < renderer->swapchain.img_count; ++i)
    vkDestroyImageView(renderer->device.logical, renderer->swapchain.views[i],
                       NULL);

  vkDestroySwapchainKHR(renderer->device.logical, renderer->swapchain.handle,
                        NULL);
}

OWL_INTERNAL enum owl_code
owl_init_transient_pool_(struct owl_renderer *renderer) {
  VkCommandPoolCreateInfo pool;

  pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool.pNext = NULL;
  pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  pool.queueFamilyIndex =
      renderer->device.families[OWL_VK_QUEUE_TYPE_GRAPHICS];

  OWL_VK_CHECK(vkCreateCommandPool(renderer->device.logical, &pool, NULL,
                                   &renderer->transient_pool));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_transient_pool_(struct owl_renderer *renderer) {
  vkDestroyCommandPool(renderer->device.logical, renderer->transient_pool,
                       NULL);
}

#define OWL_MAX_UNIFORM_SETS 16
#define OWL_MAX_SAMPLER_SETS 16
#define OWL_MAX_SETS 32

OWL_INTERNAL enum owl_code owl_init_set_pool_(struct owl_renderer *renderer) {
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

  OWL_VK_CHECK(vkCreateDescriptorPool(renderer->device.logical, &pool, NULL,
                                      &renderer->set_pool));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_set_pool_(struct owl_renderer *renderer) {
  vkDestroyDescriptorPool(renderer->device.logical, renderer->set_pool, NULL);
}

OWL_INTERNAL enum owl_code owl_init_cmd_(struct owl_renderer *renderer) {
  int i;

  renderer->cmd.active = 0;

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
    VkCommandPoolCreateInfo pool;

    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.pNext = NULL;
    pool.flags = 0;
    pool.queueFamilyIndex =
        renderer->device.families[OWL_VK_QUEUE_TYPE_GRAPHICS];

    OWL_VK_CHECK(vkCreateCommandPool(renderer->device.logical, &pool, NULL,
                                     &renderer->cmd.pools[i]));
  }

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
    VkCommandBufferAllocateInfo buf;

    buf.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buf.pNext = NULL;
    buf.commandPool = renderer->cmd.pools[i];
    buf.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buf.commandBufferCount = 1;

    OWL_VK_CHECK(vkAllocateCommandBuffers(renderer->device.logical, &buf,
                                          &renderer->cmd.bufs[i]));
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_cmd_(struct owl_renderer *renderer) {
  int i;

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkFreeCommandBuffers(renderer->device.logical, renderer->cmd.pools[i], 1,
                         &renderer->cmd.bufs[i]);

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyCommandPool(renderer->device.logical, renderer->cmd.pools[i],
                         NULL);
}

OWL_INTERNAL enum owl_code owl_init_sync_(struct owl_renderer *renderer) {
  int i;

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore;
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore.pNext = NULL;
    semaphore.flags = 0;
    OWL_VK_CHECK(vkCreateSemaphore(renderer->device.logical, &semaphore, NULL,
                                   &renderer->sync.img_available[i]));
  }

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
    VkSemaphoreCreateInfo semaphore;
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore.pNext = NULL;
    semaphore.flags = 0;
    OWL_VK_CHECK(vkCreateSemaphore(renderer->device.logical, &semaphore, NULL,
                                   &renderer->sync.render_finished[i]));
  }

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
    VkFenceCreateInfo fence;
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.pNext = NULL;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    OWL_VK_CHECK(vkCreateFence(renderer->device.logical, &fence, NULL,
                               &renderer->sync.in_flight[i]));
  }

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_sync_(struct owl_renderer *renderer) {
  int i;

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(renderer->device.logical,
                       renderer->sync.img_available[i], NULL);

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroySemaphore(renderer->device.logical,
                       renderer->sync.render_finished[i], NULL);

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyFence(renderer->device.logical, renderer->sync.in_flight[i],
                   NULL);
}

OWL_INTERNAL enum owl_code
owl_init_uniform_layout_(struct owl_renderer *renderer) {
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

  OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device.logical, &layout,
                                           NULL, &renderer->uniform_layout));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_uniform_layout_(struct owl_renderer *renderer) {
  vkDestroyDescriptorSetLayout(renderer->device.logical,
                               renderer->uniform_layout, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_texture_layout_(struct owl_renderer *renderer) {
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

  OWL_VK_CHECK(vkCreateDescriptorSetLayout(renderer->device.logical, &layout,
                                           NULL, &renderer->texture_layout));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_texture_layout_(struct owl_renderer *renderer) {
  vkDestroyDescriptorSetLayout(renderer->device.logical,
                               renderer->texture_layout, NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_layout_(struct owl_renderer *renderer) {
  VkPipelineLayoutCreateInfo layout;
  VkDescriptorSetLayout sets[2];

  sets[0] = renderer->uniform_layout;
  sets[1] = renderer->texture_layout;

  layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout.pNext = NULL;
  layout.flags = 0;
  layout.setLayoutCount = OWL_ARRAY_SIZE(sets);
  layout.pSetLayouts = sets;
  layout.pushConstantRangeCount = 0;
  layout.pPushConstantRanges = NULL;

  OWL_VK_CHECK(vkCreatePipelineLayout(renderer->device.logical, &layout, NULL,
                                      &renderer->main_layout));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_main_layout_(struct owl_renderer *renderer) {
  vkDestroyPipelineLayout(renderer->device.logical, renderer->main_layout,
                          NULL);
}

OWL_INTERNAL enum owl_code
owl_init_basic_vertex_(struct owl_renderer *renderer) {
  VkShaderModuleCreateInfo shader;

  OWL_LOCAL_PERSIST OwlU32 const code[] = {
#include <shaders/basic_vert.spv.u32>
  };

  shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader.pNext = NULL;
  shader.flags = 0;
  shader.codeSize = sizeof(code);
  shader.pCode = code;

  OWL_VK_CHECK(vkCreateShaderModule(renderer->device.logical, &shader, NULL,
                                    &renderer->basic_vertex));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_basic_vertex_(struct owl_renderer *renderer) {
  vkDestroyShaderModule(renderer->device.logical, renderer->basic_vertex,
                        NULL);
}

OWL_INTERNAL enum owl_code
owl_init_basic_fragment_(struct owl_renderer *renderer) {
  VkShaderModuleCreateInfo shader;

  OWL_LOCAL_PERSIST OwlU32 const code[] = {
#include <shaders/basic_frag.spv.u32>
  };

  shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader.pNext = NULL;
  shader.flags = 0;
  shader.codeSize = sizeof(code);
  shader.pCode = code;

  OWL_VK_CHECK(vkCreateShaderModule(renderer->device.logical, &shader, NULL,
                                    &renderer->basic_fragment));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_basic_fragment_(struct owl_renderer *renderer) {
  vkDestroyShaderModule(renderer->device.logical, renderer->basic_fragment,
                        NULL);
}

OWL_INTERNAL enum owl_code
owl_init_font_fragment_(struct owl_renderer *renderer) {
  VkShaderModuleCreateInfo shader;

  OWL_LOCAL_PERSIST OwlU32 const code[] = {
#include <shaders/font_frag.spv.u32>
  };

  shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader.pNext = NULL;
  shader.flags = 0;
  shader.codeSize = sizeof(code);
  shader.pCode = code;

  OWL_VK_CHECK(vkCreateShaderModule(renderer->device.logical, &shader, NULL,
                                    &renderer->font_fragment));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_font_fragment_(struct owl_renderer *renderer) {
  vkDestroyShaderModule(renderer->device.logical, renderer->font_fragment,
                        NULL);
}

OWL_INTERNAL enum owl_code
owl_init_main_pipeline_(struct owl_renderer *renderer) {
  VkVertexInputBindingDescription input_binding;
  VkVertexInputAttributeDescription input_attr[3];
  VkPipelineVertexInputStateCreateInfo input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterization;
  VkPipelineMultisampleStateCreateInfo multisample;
  VkPipelineColorBlendAttachmentState color_blend_attach;
  VkPipelineColorBlendStateCreateInfo color_blend;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineShaderStageCreateInfo stages[2];
  VkGraphicsPipelineCreateInfo pipeline;

  input_binding.binding = 0;
  input_binding.stride = sizeof(struct owl_vertex);
  input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  input_attr[0].binding = 0;
  input_attr[0].location = 0;
  input_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[0].offset = offsetof(struct owl_vertex, pos);
  input_attr[1].binding = 0;
  input_attr[1].location = 1;
  input_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[1].offset = offsetof(struct owl_vertex, color);
  input_attr[2].binding = 0;
  input_attr[2].location = 2;
  input_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
  input_attr[2].offset = offsetof(struct owl_vertex, uv);

  input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  input.pNext = NULL;
  input.flags = 0;
  input.vertexBindingDescriptionCount = 1;
  input.pVertexBindingDescriptions = &input_binding;
  input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(input_attr);
  input.pVertexAttributeDescriptions = input_attr;

  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext = NULL;
  input_assembly.flags = 0;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = renderer->swapchain.extent.width;
  viewport.height = renderer->swapchain.extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = renderer->swapchain.extent;

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
  multisample.rasterizationSamples = renderer->support.samples;
  multisample.sampleShadingEnable = VK_FALSE;
  multisample.minSampleShading = 1.0F;
  multisample.pSampleMask = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                 \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                     \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

  color_blend_attach.blendEnable = VK_FALSE;
  color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

  color_blend.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.logicOp = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount = 1;
  color_blend.pAttachments = &color_blend_attach;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext = NULL;
  depth_stencil.flags = 0;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  OWL_MEMSET(&depth_stencil.front, 0, sizeof(depth_stencil.front));

  OWL_MEMSET(&depth_stencil.back, 0, sizeof(depth_stencil.back));

  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext = NULL;
  stages[0].flags = 0;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = renderer->basic_vertex;
  stages[0].pName = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext = NULL;
  stages[1].flags = 0;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = renderer->basic_fragment;
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
  pipeline.pDepthStencilState = &depth_stencil;
  pipeline.pColorBlendState = &color_blend;
  pipeline.pDynamicState = NULL;
  pipeline.layout = renderer->main_layout;
  pipeline.renderPass = renderer->main_pass;
  pipeline.subpass = 0;
  pipeline.basePipelineHandle = NULL;
  pipeline.basePipelineIndex = -1;

  OWL_VK_CHECK(vkCreateGraphicsPipelines(
      renderer->device.logical, VK_NULL_HANDLE, 1, &pipeline, NULL,
      &renderer->pipeline.as[OWL_PIPELINE_TYPE_MAIN]));

  renderer->pipeline.layout[OWL_PIPELINE_TYPE_MAIN] = renderer->main_layout;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_main_pipeline_(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device.logical,
                    renderer->pipeline.as[OWL_PIPELINE_TYPE_MAIN], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_wires_pipeline_(struct owl_renderer *renderer) {
  VkVertexInputBindingDescription input_binding;
  VkVertexInputAttributeDescription input_attr[3];
  VkPipelineVertexInputStateCreateInfo input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterization;
  VkPipelineMultisampleStateCreateInfo multisample;
  VkPipelineColorBlendAttachmentState color_blend_attach;
  VkPipelineColorBlendStateCreateInfo color_blend;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineShaderStageCreateInfo stages[2];
  VkGraphicsPipelineCreateInfo pipeline;

  input_binding.binding = 0;
  input_binding.stride = sizeof(struct owl_vertex);
  input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  input_attr[0].binding = 0;
  input_attr[0].location = 0;
  input_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[0].offset = offsetof(struct owl_vertex, pos);
  input_attr[1].binding = 0;
  input_attr[1].location = 1;
  input_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[1].offset = offsetof(struct owl_vertex, color);
  input_attr[2].binding = 0;
  input_attr[2].location = 2;
  input_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
  input_attr[2].offset = offsetof(struct owl_vertex, uv);

  input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  input.pNext = NULL;
  input.flags = 0;
  input.vertexBindingDescriptionCount = 1;
  input.pVertexBindingDescriptions = &input_binding;
  input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(input_attr);
  input.pVertexAttributeDescriptions = input_attr;

  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext = NULL;
  input_assembly.flags = 0;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = renderer->swapchain.extent.width;
  viewport.height = renderer->swapchain.extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = renderer->swapchain.extent;

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
  multisample.rasterizationSamples = renderer->support.samples;
  multisample.sampleShadingEnable = VK_FALSE;
  multisample.minSampleShading = 1.0F;
  multisample.pSampleMask = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                 \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                     \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

  color_blend_attach.blendEnable = VK_FALSE;
  color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

  color_blend.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.logicOp = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount = 1;
  color_blend.pAttachments = &color_blend_attach;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext = NULL;
  depth_stencil.flags = 0;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  OWL_MEMSET(&depth_stencil.front, 0, sizeof(depth_stencil.front));

  OWL_MEMSET(&depth_stencil.back, 0, sizeof(depth_stencil.back));

  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext = NULL;
  stages[0].flags = 0;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = renderer->basic_vertex;
  stages[0].pName = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext = NULL;
  stages[1].flags = 0;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = renderer->basic_fragment;
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
  pipeline.pDepthStencilState = &depth_stencil;
  pipeline.pColorBlendState = &color_blend;
  pipeline.pDynamicState = NULL;
  pipeline.layout = renderer->main_layout;
  pipeline.renderPass = renderer->main_pass;
  pipeline.subpass = 0;
  pipeline.basePipelineHandle = NULL;
  pipeline.basePipelineIndex = -1;

  OWL_VK_CHECK(vkCreateGraphicsPipelines(
      renderer->device.logical, VK_NULL_HANDLE, 1, &pipeline, NULL,
      &renderer->pipeline.as[OWL_PIPELINE_TYPE_WIRES]));

  renderer->pipeline.layout[OWL_PIPELINE_TYPE_WIRES] = renderer->main_layout;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_wires_pipeline_(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device.logical,
                    renderer->pipeline.as[OWL_PIPELINE_TYPE_WIRES], NULL);
}

OWL_INTERNAL enum owl_code
owl_init_font_pipeline_(struct owl_renderer *renderer) {
  VkVertexInputBindingDescription input_binding;
  VkVertexInputAttributeDescription input_attr[3];
  VkPipelineVertexInputStateCreateInfo input;
  VkPipelineInputAssemblyStateCreateInfo input_assembly;
  VkViewport viewport;
  VkRect2D scissor;
  VkPipelineViewportStateCreateInfo viewport_state;
  VkPipelineRasterizationStateCreateInfo rasterization;
  VkPipelineMultisampleStateCreateInfo multisample;
  VkPipelineColorBlendAttachmentState color_blend_attach;
  VkPipelineColorBlendStateCreateInfo color_blend;
  VkPipelineDepthStencilStateCreateInfo depth_stencil;
  VkPipelineShaderStageCreateInfo stages[2];
  VkGraphicsPipelineCreateInfo pipeline;

  input_binding.binding = 0;
  input_binding.stride = sizeof(struct owl_vertex);
  input_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  input_attr[0].binding = 0;
  input_attr[0].location = 0;
  input_attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[0].offset = offsetof(struct owl_vertex, pos);
  input_attr[1].binding = 0;
  input_attr[1].location = 1;
  input_attr[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  input_attr[1].offset = offsetof(struct owl_vertex, color);
  input_attr[2].binding = 0;
  input_attr[2].location = 2;
  input_attr[2].format = VK_FORMAT_R32G32_SFLOAT;
  input_attr[2].offset = offsetof(struct owl_vertex, uv);

  input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  input.pNext = NULL;
  input.flags = 0;
  input.vertexBindingDescriptionCount = 1;
  input.pVertexBindingDescriptions = &input_binding;
  input.vertexAttributeDescriptionCount = OWL_ARRAY_SIZE(input_attr);
  input.pVertexAttributeDescriptions = input_attr;

  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.pNext = NULL;
  input_assembly.flags = 0;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  viewport.x = 0.0F;
  viewport.y = 0.0F;
  viewport.width = renderer->swapchain.extent.width;
  viewport.height = renderer->swapchain.extent.height;
  viewport.minDepth = 0.0F;
  viewport.maxDepth = 1.0F;

  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = renderer->swapchain.extent;

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
  multisample.rasterizationSamples = renderer->support.samples;
  multisample.sampleShadingEnable = VK_FALSE;
  multisample.minSampleShading = 1.0F;
  multisample.pSampleMask = NULL;
  multisample.alphaToCoverageEnable = VK_FALSE;
  multisample.alphaToOneEnable = VK_FALSE;

#define OWL_COLOR_WRITE_MASK                                                 \
  (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |                     \
   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)

  color_blend_attach.blendEnable = VK_TRUE;
  color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attach.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attach.dstAlphaBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.colorWriteMask = OWL_COLOR_WRITE_MASK;

#undef OWL_COLOR_WRITE_MASK

  color_blend.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend.pNext = NULL;
  color_blend.flags = 0;
  color_blend.logicOpEnable = VK_FALSE;
  color_blend.logicOp = VK_LOGIC_OP_COPY;
  color_blend.attachmentCount = 1;
  color_blend.pAttachments = &color_blend_attach;
  color_blend.blendConstants[0] = 0.0F;
  color_blend.blendConstants[1] = 0.0F;
  color_blend.blendConstants[2] = 0.0F;
  color_blend.blendConstants[3] = 0.0F;

  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.pNext = NULL;
  depth_stencil.flags = 0;
  depth_stencil.depthTestEnable = VK_TRUE;
  depth_stencil.depthWriteEnable = VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;
  depth_stencil.minDepthBounds = 0.0F;
  depth_stencil.maxDepthBounds = 1.0F;

  OWL_MEMSET(&depth_stencil.front, 0, sizeof(depth_stencil.front));

  OWL_MEMSET(&depth_stencil.back, 0, sizeof(depth_stencil.back));

  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].pNext = NULL;
  stages[0].flags = 0;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = renderer->basic_vertex;
  stages[0].pName = "main";
  stages[0].pSpecializationInfo = NULL;

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].pNext = NULL;
  stages[1].flags = 0;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = renderer->font_fragment;
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
  pipeline.pDepthStencilState = &depth_stencil;
  pipeline.pColorBlendState = &color_blend;
  pipeline.pDynamicState = NULL;
  pipeline.layout = renderer->main_layout;
  pipeline.renderPass = renderer->main_pass;
  pipeline.subpass = 0;
  pipeline.basePipelineHandle = NULL;
  pipeline.basePipelineIndex = -1;

  OWL_VK_CHECK(vkCreateGraphicsPipelines(
      renderer->device.logical, VK_NULL_HANDLE, 1, &pipeline, NULL,
      &renderer->pipeline.as[OWL_PIPELINE_TYPE_FONT]));

  renderer->pipeline.layout[OWL_PIPELINE_TYPE_FONT] = renderer->main_layout;

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_font_pipeline_(struct owl_renderer *renderer) {
  vkDestroyPipeline(renderer->device.logical,
                    renderer->pipeline.as[OWL_PIPELINE_TYPE_FONT], NULL);
}

#define OWL_MAX_ANISOTROPY 16

OWL_INTERNAL enum owl_code
owl_init_linear_sampler_(struct owl_renderer *renderer) {
  VkSamplerCreateInfo sampler;

  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.pNext = NULL;
  sampler.flags = 0;
  sampler.magFilter = VK_FILTER_LINEAR;
  sampler.minFilter = VK_FILTER_LINEAR;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler.mipLodBias = 0.0F;
  sampler.anisotropyEnable = VK_TRUE;
  sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
  sampler.compareEnable = VK_FALSE;
  sampler.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler.minLod = 0.0F;
  sampler.maxLod = VK_LOD_CLAMP_NONE;
  sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler.unnormalizedCoordinates = VK_FALSE;

  OWL_VK_CHECK(
      vkCreateSampler(renderer->device.logical, &sampler, NULL,
                      &renderer->sampler.as[OWL_SAMPLER_TYPE_LINEAR]));

  return OWL_SUCCESS;
}

#undef OWL_MAX_ANISOTROPY

OWL_INTERNAL void owl_deinit_linear_sampler_(struct owl_renderer *renderer) {
  vkDestroySampler(renderer->device.logical,
                   renderer->sampler.as[OWL_SAMPLER_TYPE_LINEAR], NULL);
}

#define OWL_MAX_ANISOTROPY 16

OWL_INTERNAL enum owl_code
owl_init_nearest_sampler_(struct owl_renderer *renderer) {
  VkSamplerCreateInfo sampler;

  sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler.pNext = NULL;
  sampler.flags = 0;
  sampler.magFilter = VK_FILTER_NEAREST;
  sampler.minFilter = VK_FILTER_NEAREST;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.mipLodBias = 0.0F;
  sampler.anisotropyEnable = VK_TRUE;
  sampler.maxAnisotropy = OWL_MAX_ANISOTROPY;
  sampler.compareEnable = VK_FALSE;
  sampler.compareOp = VK_COMPARE_OP_ALWAYS;
  sampler.minLod = 0.0F;
  sampler.maxLod = VK_LOD_CLAMP_NONE;
  sampler.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler.unnormalizedCoordinates = VK_FALSE;

  OWL_VK_CHECK(
      vkCreateSampler(renderer->device.logical, &sampler, NULL,
                      &renderer->sampler.as[OWL_SAMPLER_TYPE_NEAREST]));

  return OWL_SUCCESS;
}

#undef OWL_MAX_ANISOTROPY

OWL_INTERNAL void owl_deinit_nearest_sampler_(struct owl_renderer *renderer) {
  vkDestroySampler(renderer->device.logical,
                   renderer->sampler.as[OWL_SAMPLER_TYPE_NEAREST], NULL);
}

OWL_INTERNAL enum owl_code owl_init_dbl_buf_(struct owl_renderer *renderer,
                                             OwlDeviceSize size) {
  enum owl_code err = OWL_SUCCESS;

  renderer->dbl_buf.active = 0;
  renderer->dbl_buf.size = size;

  /* init buffers */
  {
#define OWL_DOUBLE_BUFFER_USAGE                                              \
  (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |    \
   VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)

    int i;
    VkBufferCreateInfo buf;
    buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf.pNext = NULL;
    buf.flags = 0;
    buf.size = size;
    buf.usage = OWL_DOUBLE_BUFFER_USAGE;
    buf.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buf.queueFamilyIndexCount = 0;
    buf.pQueueFamilyIndices = NULL;

    for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_VK_CHECK(vkCreateBuffer(renderer->device.logical, &buf, NULL,
                                  &renderer->dbl_buf.bufs[i]));

#undef OWL_DOUBLE_BUFFER_USAGE
  }

  /* init memory */
  {
    OtterVkMemoryType type;
    VkMemoryRequirements req;
    VkMemoryAllocateInfo mem;

    vkGetBufferMemoryRequirements(renderer->device.logical,
                                  renderer->dbl_buf.bufs[0], &req);

    if (OWL_VK_MEMORY_TYPE_NONE ==
        (type = owl_vk_find_mem_type(renderer, req.memoryTypeBits,
                                     OWL_VK_MEMORY_VISIBILITY_CPU_ONLY)))
      goto end;

    renderer->dbl_buf.alignment = req.alignment;
    renderer->dbl_buf.aligned_size = OWL_ALIGN(size, req.alignment);

    mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem.pNext = NULL;
    mem.allocationSize =
        renderer->dbl_buf.aligned_size * OWL_DYNAMIC_BUFFER_COUNT;
    mem.memoryTypeIndex = type;

    OWL_VK_CHECK(vkAllocateMemory(renderer->device.logical, &mem, NULL,
                                  &renderer->dbl_buf.mem));
  }

  /* bind buffers to memory */
  {
    int i;
    for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
      OWL_VK_CHECK(
          vkBindBufferMemory(renderer->device.logical,
                             renderer->dbl_buf.bufs[i], renderer->dbl_buf.mem,
                             (unsigned)i * renderer->dbl_buf.aligned_size));
  }

  /* map memory */
  {
    OWL_VK_CHECK(vkMapMemory(renderer->device.logical, renderer->dbl_buf.mem,
                             0, VK_WHOLE_SIZE, 0, &renderer->dbl_buf.data));
  }

  {
    int i;
    VkDescriptorSetAllocateInfo sets;
    VkDescriptorSetLayout layouts[OWL_DYNAMIC_BUFFER_COUNT];

    for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
      layouts[i] = renderer->uniform_layout;

    sets.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sets.pNext = NULL;
    sets.descriptorPool = renderer->set_pool;
    sets.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
    sets.pSetLayouts = layouts;

    OWL_VK_CHECK(vkAllocateDescriptorSets(renderer->device.logical, &sets,
                                          renderer->dbl_buf.sets));
  }

  /* write the descriptor sets */
  {
    int i;
    for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buf;

      buf.buffer = renderer->dbl_buf.bufs[i];
      buf.offset = 0;
      buf.range = sizeof(struct owl_uniform);

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = NULL;
      write.dstSet = renderer->dbl_buf.sets[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
      write.pImageInfo = NULL;
      write.pBufferInfo = &buf;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(renderer->device.logical, 1, &write, 0, NULL);
    }
  }

  /* zero the offsets */
  {
    int i;
    for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
      renderer->dbl_buf.offsets[i] = 0;
  }

end:
  return err;
}

OWL_INTERNAL void owl_deinit_dbl_buf_(struct owl_renderer *renderer) {
  int i;

  /* kind of redundant */
  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkFreeDescriptorSets(renderer->device.logical, renderer->set_pool, 1,
                         &renderer->dbl_buf.sets[i]);

  vkFreeMemory(renderer->device.logical, renderer->dbl_buf.mem, NULL);

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    vkDestroyBuffer(renderer->device.logical, renderer->dbl_buf.bufs[i],
                    NULL);
}

OWL_INTERNAL enum owl_code owl_init_garbage_(struct owl_renderer *renderer) {
  renderer->garbage.buf_count = 0;
  renderer->garbage.mem_count = 0;
  renderer->garbage.set_count = 0;

  OWL_MEMSET(renderer->garbage.bufs, 0, sizeof(renderer->garbage.bufs));
  OWL_MEMSET(renderer->garbage.mems, 0, sizeof(renderer->garbage.mems));
  OWL_MEMSET(renderer->garbage.sets, 0, sizeof(renderer->garbage.sets));

  return OWL_SUCCESS;
}

OWL_INTERNAL void owl_deinit_garbage_(struct owl_renderer *renderer) {
  int i;

  for (i = 0; i < renderer->garbage.set_count; ++i)
    vkFreeDescriptorSets(renderer->device.logical, renderer->set_pool, 1,
                         &renderer->garbage.sets[i]);

  renderer->garbage.set_count = 0;

  for (i = 0; i < renderer->garbage.mem_count; ++i)
    vkFreeMemory(renderer->device.logical, renderer->garbage.mems[i], NULL);

  renderer->garbage.mem_count = 0;

  for (i = 0; i < renderer->garbage.buf_count; ++i)
    vkDestroyBuffer(renderer->device.logical, renderer->garbage.bufs[i],
                    NULL);

  renderer->garbage.buf_count = 0;
}

enum owl_code
owl_init_renderer(struct owl_extent const *extent,
                  struct owl_vk_surface_provider const *provider,
                  struct owl_vk_extensions const *extensions,
                  struct owl_renderer *renderer) {
  enum owl_code err;

  renderer->extent = *extent;

  if (OWL_SUCCESS != (err = owl_init_instance_(extensions, renderer)))
    goto end;

/* FIXME(samuel): if surface fails debug isnt' destroyed */
#ifdef OWL_ENABLE_VALIDATION
  if (OWL_SUCCESS != (err = owl_init_dbg_(renderer)))
    goto end;
#endif

  if (OWL_SUCCESS != (err = owl_init_surface_(provider, renderer)))
    goto end_deinit_instance;

  if (OWL_SUCCESS != (err = owl_init_device_(renderer)))
    goto end_deinit_surface;

  if (OWL_SUCCESS != (err = owl_init_main_pass_(renderer)))
    goto end_deinit_device;

  if (OWL_SUCCESS != (err = owl_init_color_attach_(extent, renderer)))
    goto end_deinit_main_pass;

  if (OWL_SUCCESS != (err = owl_init_depth_attach_(extent, renderer)))
    goto end_deinit_color_attach;

  if (OWL_SUCCESS != (err = owl_init_swapchain_(extent, renderer)))
    goto end_deinit_depth_attach;

  if (OWL_SUCCESS != (err = owl_init_transient_pool_(renderer)))
    goto end_deinit_swapchain;

  if (OWL_SUCCESS != (err = owl_init_set_pool_(renderer)))
    goto end_deinit_transient_pool;

  if (OWL_SUCCESS != (err = owl_init_cmd_(renderer)))
    goto end_deinit_set_pool;

  if (OWL_SUCCESS != (err = owl_init_sync_(renderer)))
    goto end_deinit_cmd;

  if (OWL_SUCCESS != (err = owl_init_uniform_layout_(renderer)))
    goto end_deinit_sync;

  if (OWL_SUCCESS != (err = owl_init_texture_layout_(renderer)))
    goto end_deinit_uniform_layout;

  if (OWL_SUCCESS != (err = owl_init_main_layout_(renderer)))
    goto end_deinit_texture_layout;

  if (OWL_SUCCESS != (err = owl_init_basic_vertex_(renderer)))
    goto end_deinit_main_layout;

  if (OWL_SUCCESS != (err = owl_init_basic_fragment_(renderer)))
    goto end_deinit_basic_vertex;

  if (OWL_SUCCESS != (err = owl_init_font_fragment_(renderer)))
    goto end_deinit_basic_fragment;

  if (OWL_SUCCESS != (err = owl_init_main_pipeline_(renderer)))
    goto end_deinit_font_fragment;

  if (OWL_SUCCESS != (err = owl_init_wires_pipeline_(renderer)))
    goto end_deinit_main_pipeline;

  if (OWL_SUCCESS != (err = owl_init_font_pipeline_(renderer)))
    goto end_deinit_wires_pipeline;

  if (OWL_SUCCESS != (err = owl_init_linear_sampler_(renderer)))
    goto end_deinit_font_pipeline;

  if (OWL_SUCCESS != (err = owl_init_nearest_sampler_(renderer)))
    goto end_deinit_linear_sampler;

  if (OWL_SUCCESS != (err = owl_init_dbl_buf_(renderer, OWL_MB)))
    goto end_deinit_nearest_sampler;

  if (OWL_SUCCESS != (err = owl_init_garbage_(renderer)))
    goto end_deinit_dbl_buf;

  goto end;

end_deinit_dbl_buf:
  owl_deinit_dbl_buf_(renderer);

end_deinit_nearest_sampler:
  owl_deinit_nearest_sampler_(renderer);

end_deinit_linear_sampler:
  owl_deinit_linear_sampler_(renderer);

end_deinit_font_pipeline:
  owl_deinit_font_pipeline_(renderer);

end_deinit_wires_pipeline:
  owl_deinit_wires_pipeline_(renderer);

end_deinit_main_pipeline:
  owl_deinit_main_pipeline_(renderer);

end_deinit_font_fragment:
  owl_deinit_font_fragment_(renderer);

end_deinit_basic_fragment:
  owl_deinit_basic_fragment_(renderer);

end_deinit_basic_vertex:
  owl_deinit_basic_vertex_(renderer);

end_deinit_main_layout:
  owl_deinit_main_layout_(renderer);

end_deinit_texture_layout:
  owl_deinit_texture_layout_(renderer);

end_deinit_uniform_layout:
  owl_deinit_uniform_layout_(renderer);

end_deinit_sync:
  owl_deinit_sync_(renderer);

end_deinit_cmd:
  owl_deinit_cmd_(renderer);

end_deinit_set_pool:
  owl_deinit_set_pool_(renderer);

end_deinit_transient_pool:
  owl_deinit_transient_pool_(renderer);

end_deinit_swapchain:
  owl_deinit_swapchain_(renderer);

end_deinit_depth_attach:
  owl_deinit_depth_attach_(renderer);

end_deinit_color_attach:
  owl_deinit_color_attach_(renderer);

end_deinit_main_pass:
  owl_deinit_main_pass_(renderer);

end_deinit_device:
  owl_deinit_device_(renderer);

end_deinit_surface:
  owl_deinit_surface_(renderer);

end_deinit_instance:
  owl_deinit_instance_(renderer);

end:
  return err;
}

void owl_deinit_renderer(struct owl_renderer *renderer) {
  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device.logical));

  owl_deinit_garbage_(renderer);
  owl_deinit_dbl_buf_(renderer);
  owl_deinit_nearest_sampler_(renderer);
  owl_deinit_linear_sampler_(renderer);
  owl_deinit_font_pipeline_(renderer);
  owl_deinit_wires_pipeline_(renderer);
  owl_deinit_main_pipeline_(renderer);
  owl_deinit_font_fragment_(renderer);
  owl_deinit_basic_fragment_(renderer);
  owl_deinit_basic_vertex_(renderer);
  owl_deinit_main_layout_(renderer);
  owl_deinit_texture_layout_(renderer);
  owl_deinit_uniform_layout_(renderer);
  owl_deinit_sync_(renderer);
  owl_deinit_cmd_(renderer);
  owl_deinit_set_pool_(renderer);
  owl_deinit_transient_pool_(renderer);
  owl_deinit_swapchain_(renderer);
  owl_deinit_depth_attach_(renderer);
  owl_deinit_color_attach_(renderer);
  owl_deinit_main_pass_(renderer);
  owl_deinit_device_(renderer);
  owl_deinit_surface_(renderer);

#ifdef OWL_ENABLE_VALIDATION
  owl_deinit_dbg_(renderer);
#endif

  owl_deinit_instance_(renderer);
}

enum owl_code owl_create_renderer(struct owl_window const *window,
                                  struct owl_renderer **renderer) {
  struct owl_vk_surface_provider provider;
  struct owl_vk_extensions extensions;

  owl_vk_fill_info(window, &provider, &extensions);

  *renderer = OWL_MALLOC(sizeof(**renderer));

  return owl_init_renderer(&window->framebuffer, &provider, &extensions,
                           *renderer);
}

void owl_destroy_renderer(struct owl_renderer *renderer) {
  owl_deinit_renderer(renderer);
  OWL_FREE(renderer);
}

OWL_INTERNAL enum owl_code
owl_move_buf_to_garbage_(struct owl_renderer *renderer) {
  int i, count;
  enum owl_code err = OWL_SUCCESS;

  if (OWL_MAX_GARBAGE_ITEMS <=
      (count = renderer->garbage.buf_count + OWL_DYNAMIC_BUFFER_COUNT)) {
    err = OWL_ERROR_UNKNOWN;
    goto end;
  }

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    renderer->garbage.bufs[renderer->garbage.buf_count + i] =
        renderer->dbl_buf.bufs[i];

  renderer->garbage.buf_count = count;

  if (OWL_MAX_GARBAGE_ITEMS <=
      (count = renderer->garbage.set_count + OWL_DYNAMIC_BUFFER_COUNT)) {
    err = OWL_ERROR_UNKNOWN;
    goto end;
  }

  for (i = 0; i < OWL_DYNAMIC_BUFFER_COUNT; ++i)
    renderer->garbage.sets[renderer->garbage.set_count + i] =
        renderer->dbl_buf.sets[i];

  renderer->garbage.set_count = count;

  if (OWL_MAX_GARBAGE_ITEMS <= (count = renderer->garbage.set_count + 1)) {
    err = OWL_ERROR_UNKNOWN;
    goto end;
  }

  renderer->garbage.mems[renderer->garbage.set_count + 0] =
      renderer->dbl_buf.mem;

  renderer->garbage.mem_count = count;

end:
  return err;
}

enum owl_code owl_reseve_dbl_buf_mem(struct owl_renderer *renderer,
                                     OwlDeviceSize size) {
  enum owl_code err = OWL_SUCCESS;
  int const active = renderer->dbl_buf.active;
  OwlDeviceSize required = renderer->dbl_buf.offsets[active] + size;

  if (required > renderer->dbl_buf.size) {
    vkUnmapMemory(renderer->device.logical, renderer->dbl_buf.mem);

    if (OWL_SUCCESS != (err = owl_move_buf_to_garbage_(renderer)))
      goto end;

    if (OWL_SUCCESS != (err = owl_init_dbl_buf_(renderer, 2 * required)))
      goto end;
  }

end:
  return err;
}

enum owl_code owl_reinit_renderer(struct owl_extent const *extent,
                                  struct owl_renderer *renderer) {
  enum owl_code err = OWL_SUCCESS;

  OWL_VK_CHECK(vkDeviceWaitIdle(renderer->device.logical));

  renderer->extent = *extent;

  owl_deinit_font_pipeline_(renderer);
  owl_deinit_wires_pipeline_(renderer);
  owl_deinit_main_pipeline_(renderer);
  owl_deinit_sync_(renderer);
  owl_deinit_swapchain_(renderer);
  owl_deinit_depth_attach_(renderer);
  owl_deinit_color_attach_(renderer);
  owl_deinit_main_pass_(renderer);

  if (OWL_SUCCESS != (err = owl_init_main_pass_(renderer)))
    goto end;

  if (OWL_SUCCESS != (err = owl_init_color_attach_(extent, renderer)))
    goto end_deinit_main_pass;

  if (OWL_SUCCESS != (err = owl_init_depth_attach_(extent, renderer)))
    goto end_deinit_color_attach;

  if (OWL_SUCCESS != (err = owl_init_swapchain_(extent, renderer)))
    goto end_deinit_depth_attach;

  if (OWL_SUCCESS != (err = owl_init_sync_(renderer)))
    goto end_deinit_swapchain;

  if (OWL_SUCCESS != (err = owl_init_main_pipeline_(renderer)))
    goto end_deinit_sync;

  if (OWL_SUCCESS != (err = owl_init_wires_pipeline_(renderer)))
    goto end_deinit_main_pipeline;

  if (OWL_SUCCESS != (err = owl_init_font_pipeline_(renderer)))
    goto end_deinit_wires_pipeline;

  goto end;

end_deinit_wires_pipeline:
  owl_deinit_wires_pipeline_(renderer);

end_deinit_main_pipeline:
  owl_deinit_main_pipeline_(renderer);

end_deinit_sync:
  owl_deinit_sync_(renderer);

end_deinit_swapchain:
  owl_deinit_swapchain_(renderer);

end_deinit_depth_attach:
  owl_deinit_depth_attach_(renderer);

end_deinit_color_attach:
  owl_deinit_color_attach_(renderer);

end_deinit_main_pass:
  owl_deinit_main_pass_(renderer);

end:
  return err;
}

enum owl_code owl_recreate_renderer(struct owl_window const *window,
                                    struct owl_renderer *renderer) {
  return owl_reinit_renderer(&window->framebuffer, renderer);
}

void owl_clear_garbage(struct owl_renderer *renderer) {
  owl_deinit_garbage_(renderer);
}

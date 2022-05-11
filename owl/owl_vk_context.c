#include "owl_vk_context.h"

#include "owl_internal.h"
#include "owl_window.h"

#define OWL_QUEUE_FAMILY_INDEX_NONE (owl_u32) - 1
#define OWL_ALL_SAMPLE_FLAG_BITS    (owl_u32) - 1
#define OWL_MAX_PRESENT_MODES       16
#define OWL_JUST_ENOUGH_DESCRIPTORS 256

owl_private char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(OWL_ENABLE_VALIDATION)

#include <stdio.h>
static VKAPI_ATTR VKAPI_CALL VkBool32
owl_vk_context_debug_messenger_callback (
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    VkDebugUtilsMessengerCallbackDataEXT const *data, void *user_data)
{
  struct owl_renderer const *renderer = user_data;

  owl_unused (type);
  owl_unused (renderer);

  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
    fprintf (stderr,
             "\033[32m[VALIDATION_LAYER]\033[0m \033[34m(VERBOSE)\033[0m %s\n",
             data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    fprintf (stderr,
             "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(INFO)\033[0m %s\n",
             data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    fprintf (stderr,
             "\033[32m[VALIDATION_LAYER]\033[0m \033[33m(WARNING)\033[0m %s\n",
             data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    fprintf (stderr,
             "\033[32m[VALIDATION_LAYER]\033[0m \033[31m(ERROR)\033[0m %s\n",
             data->pMessage);
  }

  return VK_FALSE;
}

owl_private char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif /* OWL_ENABLE_VALIDATION */

owl_private enum owl_code
owl_vk_context_instance_init (struct owl_vk_context *ctx,
                              struct owl_window const *w)
{
  VkApplicationInfo app;
  VkInstanceCreateInfo info;

  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pNext = NULL;
  app.pApplicationName = w->title;
  app.applicationVersion = VK_MAKE_VERSION (1, 0, 0);
  app.pEngineName = "No Engine";
  app.engineVersion = VK_MAKE_VERSION (1, 0, 0);
  app.apiVersion = VK_MAKE_VERSION (1, 0, 0);

  info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  info.pNext = NULL;
  info.flags = 0;
  info.pApplicationInfo = &app;
#if defined(OWL_ENABLE_VALIDATION)
  info.enabledLayerCount = owl_array_size (debug_validation_layers);
  info.ppEnabledLayerNames = debug_validation_layers;
#else  /* OWL_ENABLE_VALIDATION */
  info.enabledLayerCount = 0;
  info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
  info.ppEnabledExtensionNames =
      owl_window_get_instance_extensions (&info.enabledExtensionCount);

  if (VK_SUCCESS != vkCreateInstance (&info, NULL, &ctx->vk_instance))
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_context_instance_deinit (struct owl_vk_context *ctx)
{
  vkDestroyInstance (ctx->vk_instance, NULL);
  ctx->vk_instance = VK_NULL_HANDLE;
}

owl_private enum owl_code
owl_vk_context_debug_messenger_init (struct owl_vk_context *ctx)
{
  VkResult vk_result;
  VkDebugUtilsMessengerCreateInfoEXT info;
  PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;

  vkCreateDebugUtilsMessengerEXT =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr (
          ctx->vk_instance, "vkCreateDebugUtilsMessengerEXT");

  owl_assert (vkCreateDebugUtilsMessengerEXT);

  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.pNext = NULL;
  info.flags = 0;
  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = owl_vk_context_debug_messenger_callback;
  info.pUserData = ctx;

  vk_result = vkCreateDebugUtilsMessengerEXT (ctx->vk_instance, &info, NULL,
                                              &ctx->vk_debug_messenger);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private void
owl_vk_context_debug_messenger_deinit (struct owl_vk_context *ctx)
{
  PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

  vkDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr (
          ctx->vk_instance, "vkDestroyDebugUtilsMessengerEXT");

  owl_assert (vkDestroyDebugUtilsMessengerEXT);

  vkDestroyDebugUtilsMessengerEXT (ctx->vk_instance, ctx->vk_debug_messenger,
                                   NULL);
  ctx->vk_debug_messenger = VK_NULL_HANDLE;
}

owl_private enum owl_code
owl_vk_context_surface_init (struct owl_vk_context *ctx,
                             struct owl_window const *w)
{
  return owl_window_create_vk_surface (w, ctx->vk_instance, &ctx->vk_surface);
}

owl_private void
owl_vk_context_surface_deinit (struct owl_vk_context *ctx)
{
  vkDestroySurfaceKHR (ctx->vk_instance, ctx->vk_surface, NULL);
  ctx->vk_surface = VK_NULL_HANDLE;
}

owl_private enum owl_code
owl_vk_context_device_options_fill (struct owl_vk_context *ctx)
{
  VkResult vk_result;

  vk_result = vkEnumeratePhysicalDevices (ctx->vk_instance,
                                          &ctx->vk_device_option_count, NULL);
  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  if (OWL_VK_CONTEXT_MAX_DEVICE_OPTION_COUNT <= ctx->vk_device_option_count)
    return OWL_ERROR_OUT_OF_BOUNDS;

  vk_result = vkEnumeratePhysicalDevices (
      ctx->vk_instance, &ctx->vk_device_option_count, ctx->vk_device_options);

  if (VK_SUCCESS != vk_result)
    return OWL_ERROR_UNKNOWN;

  return OWL_SUCCESS;
}

owl_private owl_b32
owl_vk_context_query_families (struct owl_vk_context *ctx, owl_u32 id)
{

  owl_i32 found;
  owl_i32 i;
  owl_u32 property_count;
  VkQueueFamilyProperties *properties;

  VkResult vk_result = VK_SUCCESS;

  vkGetPhysicalDeviceQueueFamilyProperties (ctx->vk_device_options[id],
                                            &property_count, NULL);

  properties = owl_malloc (property_count * sizeof (*properties));
  if (!properties) {
    found = 0;
    goto out;
  }

  vkGetPhysicalDeviceQueueFamilyProperties (ctx->vk_device_options[id],
                                            &property_count, properties);

  ctx->graphics_queue_family = OWL_QUEUE_FAMILY_INDEX_NONE;
  ctx->present_queue_family = OWL_QUEUE_FAMILY_INDEX_NONE;

  for (i = 0; i < (owl_i32)property_count; ++i) {
    VkBool32 has_surface;

    if (!properties[i].queueCount)
      continue;

    if (VK_QUEUE_GRAPHICS_BIT & properties[i].queueFlags)
      ctx->graphics_queue_family = i;

    vk_result = vkGetPhysicalDeviceSurfaceSupportKHR (
        ctx->vk_device_options[id], i, ctx->vk_surface, &has_surface);

    if (VK_SUCCESS != vk_result) {
      found = 0;
      goto out_properties_free;
    }

    if (has_surface)
      ctx->present_queue_family = i;

    if (OWL_QUEUE_FAMILY_INDEX_NONE == ctx->graphics_queue_family)
      continue;

    if (OWL_QUEUE_FAMILY_INDEX_NONE == ctx->present_queue_family)
      continue;

    found = 1;
    goto out_properties_free;
  }

  found = 0;

out_properties_free:
  owl_free (properties);

out:
  return found;
}

owl_private int
owl_validate_device_extensions (owl_u32 extension_count,
                                VkExtensionProperties const *extensions)
{
  owl_i32 i;
  owl_i32 found = 1;
  owl_b32 extensions_found[owl_array_size (device_extensions)];

  for (i = 0; i < (owl_i32)owl_array_size (device_extensions); ++i) {
    extensions_found[i] = 0;
  }

  for (i = 0; i < (owl_i32)extension_count; ++i) {
    owl_u32 j;
    for (j = 0; j < owl_array_size (device_extensions); ++j) {
      if (!owl_strncmp (device_extensions[j], extensions[i].extensionName,
                        VK_MAX_EXTENSION_NAME_SIZE)) {
        extensions_found[j] = 1;
      }
    }
  }

  for (i = 0; i < (owl_i32)owl_array_size (device_extensions); ++i) {
    if (!extensions_found[i]) {
      found = 0;
      goto out;
    }
  }

out:
  return found;
}

owl_private enum owl_code
owl_vk_context_physical_device_select (struct owl_vk_context *ctx)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < (owl_i32)ctx->vk_device_option_count; ++i) {
    owl_b32 has_families;
    owl_b32 validated;
    owl_u32 has_formats;
    owl_u32 has_modes;
    owl_u32 extension_count;
    VkPhysicalDeviceFeatures features;
    VkExtensionProperties *extensions;

    ctx->vk_physical_device = ctx->vk_device_options[i];

    vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR (
        ctx->vk_physical_device, ctx->vk_surface, &has_formats, NULL);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }

    if (!has_formats)
      continue;

    vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR (
        ctx->vk_physical_device, ctx->vk_surface, &has_modes, NULL);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }

    if (!has_modes)
      continue;

    has_families = owl_vk_context_query_families (ctx, i);
    if (!has_families)
      continue;

    vkGetPhysicalDeviceFeatures (ctx->vk_physical_device, &features);

    if (!features.samplerAnisotropy)
      continue;

    vk_result = vkEnumerateDeviceExtensionProperties (
        ctx->vk_physical_device, NULL, &extension_count, NULL);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }

    extensions = owl_malloc (extension_count * sizeof (*extensions));
    if (!extensions) {
      code = OWL_ERROR_BAD_ALLOCATION;
      goto out;
    }

    vk_result = vkEnumerateDeviceExtensionProperties (
        ctx->vk_physical_device, NULL, &extension_count, extensions);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      owl_free (extensions);
      goto out;
    }

    validated = owl_validate_device_extensions (extension_count, extensions);
    if (!validated) {
      owl_free (extensions);
      continue;
    }

    owl_free (extensions);

    code = OWL_SUCCESS;
    goto out;
  }

  code = OWL_ERROR_NO_SUITABLE_DEVICE;

out:
  return code;
}

owl_private enum owl_code
owl_vk_context_surface_format_ensure (struct owl_vk_context const *ctx)
{
  owl_i32 i;
  owl_u32 format_count;
  VkSurfaceFormatKHR *formats;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR (
      ctx->vk_physical_device, ctx->vk_surface, &format_count, NULL);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  formats = owl_malloc (format_count * sizeof (*formats));
  if (!formats) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out;
  }

  vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR (
      ctx->vk_physical_device, ctx->vk_surface, &format_count, formats);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_formats_free;
  }

  for (i = 0; i < (owl_i32)format_count; ++i) {
    if (ctx->vk_surface_format.format != formats[i].format)
      continue;

    if (ctx->vk_surface_format.colorSpace != formats[i].colorSpace)
      continue;

    code = OWL_SUCCESS;
    goto out_formats_free;
  }

  /* if the loop ends without finding a format */
  code = OWL_ERROR_NO_SUITABLE_FORMAT;

out_formats_free:
  owl_free (formats);

out:
  return code;
}

owl_private enum owl_code
owl_vk_context_msaa_ensure (struct owl_vk_context const *ctx)
{
  VkPhysicalDeviceProperties properties;

  enum owl_code code = OWL_SUCCESS;
  VkSampleCountFlags limit = OWL_ALL_SAMPLE_FLAG_BITS;

  vkGetPhysicalDeviceProperties (ctx->vk_physical_device, &properties);

  limit &= properties.limits.framebufferColorSampleCounts;
  limit &= properties.limits.framebufferDepthSampleCounts;

  if (VK_SAMPLE_COUNT_1_BIT & ctx->msaa) {
    owl_assert (0 && "disabling multisampling is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (!(limit & ctx->msaa)) {
    owl_assert (0 && "msaa_sample_count is not supported");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_private enum owl_code
owl_vk_context_present_mode_ensure (struct owl_vk_context *ctx)
{
  owl_i32 i;
  owl_u32 mode_count;
  VkPresentModeKHR modes[OWL_MAX_PRESENT_MODES];

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  VkPresentModeKHR const preferred = ctx->vk_present_mode;

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR (
      ctx->vk_physical_device, ctx->vk_surface, &mode_count, NULL);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (OWL_MAX_PRESENT_MODES <= mode_count) {
    code = OWL_ERROR_OUT_OF_SPACE;
    goto out;
  }

  vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR (
      ctx->vk_physical_device, ctx->vk_surface, &mode_count, modes);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  for (i = 0; i < (owl_i32)mode_count; ++i) {
    ctx->vk_present_mode = modes[mode_count - i - 1];

    if (preferred == ctx->vk_present_mode)
      goto out;
  }

out:
  return code;
}

owl_private enum owl_code
owl_vk_context_device_init (struct owl_vk_context *ctx)
{
  VkPhysicalDeviceFeatures features;
  VkDeviceCreateInfo device_info;
  VkDeviceQueueCreateInfo queue_infos[2];

  float const priority = 1.0F;
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[0].pNext = NULL;
  queue_infos[0].flags = 0;
  queue_infos[0].queueFamilyIndex = ctx->graphics_queue_family;
  queue_infos[0].queueCount = 1;
  queue_infos[0].pQueuePriorities = &priority;

  queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_infos[1].pNext = NULL;
  queue_infos[1].flags = 0;
  queue_infos[1].queueFamilyIndex = ctx->present_queue_family;
  queue_infos[1].queueCount = 1;
  queue_infos[1].pQueuePriorities = &priority;

  vkGetPhysicalDeviceFeatures (ctx->vk_physical_device, &features);

  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = NULL;
  device_info.flags = 0;
  if (ctx->graphics_queue_family == ctx->present_queue_family) {
    device_info.queueCreateInfoCount = 1;
  } else {
    device_info.queueCreateInfoCount = 2;
  }
  device_info.pQueueCreateInfos = queue_infos;
  device_info.enabledLayerCount = 0;      /* deprecated */
  device_info.ppEnabledLayerNames = NULL; /* deprecated */
  device_info.enabledExtensionCount = owl_array_size (device_extensions);
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.pEnabledFeatures = &features;

  vk_result = vkCreateDevice (ctx->vk_physical_device, &device_info, NULL,
                              &ctx->vk_device);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkGetDeviceQueue (ctx->vk_device, ctx->graphics_queue_family, 0,
                    &ctx->vk_graphics_queue);

  vkGetDeviceQueue (ctx->vk_device, ctx->present_queue_family, 0,
                    &ctx->vk_present_queue);

out:
  return code;
}

owl_private void
owl_vk_context_device_deinit (struct owl_vk_context *ctx)
{
  vkDestroyDevice (ctx->vk_device, NULL);
}

owl_private VkFormat
owl_vk_context_depth_stencil_format_get (struct owl_vk_context const *ctx)
{
  VkFormatProperties properties;

  vkGetPhysicalDeviceFormatProperties (
      ctx->vk_physical_device, VK_FORMAT_D24_UNORM_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    return VK_FORMAT_D24_UNORM_S8_UINT;

  vkGetPhysicalDeviceFormatProperties (
      ctx->vk_physical_device, VK_FORMAT_D32_SFLOAT_S8_UINT, &properties);

  if (properties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    return VK_FORMAT_D32_SFLOAT_S8_UINT;

  return VK_FORMAT_D32_SFLOAT;
}

owl_private enum owl_code
owl_vk_context_depth_stencil_format_ensure (struct owl_vk_context *ctx)
{
  enum owl_code code = OWL_SUCCESS;

  ctx->vk_depth_stencil_format = owl_vk_context_depth_stencil_format_get (ctx);

  return code;
}

owl_private enum owl_code
owl_vk_context_main_render_pass_init (struct owl_vk_context *ctx)
{
  VkAttachmentReference color_reference;
  VkAttachmentReference depth_reference;
  VkAttachmentReference resolve_reference;
  VkAttachmentDescription attachments[3];
  VkSubpassDescription subpass;
  VkSubpassDependency dependency;
  VkRenderPassCreateInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  color_reference.attachment = 0;
  color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  depth_reference.attachment = 1;
  depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  resolve_reference.attachment = 2;
  resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* color */
  attachments[0].flags = 0;
  attachments[0].format = ctx->vk_surface_format.format;
  attachments[0].samples = ctx->msaa;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  /* depth */
  attachments[1].flags = 0;
  attachments[1].format = ctx->vk_depth_stencil_format;
  attachments[1].samples = ctx->msaa;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  /* color resolve */
  attachments[2].flags = 0;
  attachments[2].format = ctx->vk_surface_format.format;
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
  info.attachmentCount = owl_array_size (attachments);
  info.pAttachments = attachments;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  vk_result = vkCreateRenderPass (ctx->vk_device, &info, NULL,
                                  &ctx->vk_main_render_pass);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

out:
  return code;
}

owl_private void
owl_vk_context_main_render_pass_deinit (struct owl_vk_context *ctx)
{
  vkDestroyRenderPass (ctx->vk_device, ctx->vk_main_render_pass, NULL);
}

owl_private enum owl_code
owl_vk_context_pools_init (struct owl_vk_context *ctx)
{
  VkCommandPoolCreateInfo command_pool_info;
  VkDescriptorPoolSize sizes[6];
  VkDescriptorPoolCreateInfo set_pool_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.pNext = NULL;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  command_pool_info.queueFamilyIndex = ctx->graphics_queue_family;

  vk_result = vkCreateCommandPool (ctx->vk_device, &command_pool_info, NULL,
                                   &ctx->vk_command_pool);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

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

  set_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  set_pool_info.pNext = NULL;
  set_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  set_pool_info.maxSets = OWL_JUST_ENOUGH_DESCRIPTORS * owl_array_size (sizes);
  set_pool_info.poolSizeCount = owl_array_size (sizes);
  set_pool_info.pPoolSizes = sizes;

  vk_result = vkCreateDescriptorPool (ctx->vk_device, &set_pool_info, NULL,
                                      &ctx->vk_set_pool);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_command_pool_deinit;
  }

  goto out;

out_error_command_pool_deinit:
  vkDestroyCommandPool (ctx->vk_device, ctx->vk_command_pool, NULL);

out:
  return code;
}

owl_private void
owl_vk_context_pools_deinit (struct owl_vk_context *ctx)
{
  vkDestroyDescriptorPool (ctx->vk_device, ctx->vk_set_pool, NULL);
  vkDestroyCommandPool (ctx->vk_device, ctx->vk_command_pool, NULL);
}

owl_public enum owl_code
owl_vk_context_init (struct owl_vk_context *ctx, struct owl_window const *w)
{
  enum owl_code code;

  code = owl_vk_context_instance_init (ctx, w);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_context_debug_messenger_init (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_instance_deinit;

  code = owl_vk_context_surface_init (ctx, w);
  if (OWL_SUCCESS != code)
    goto out_error_debug_messenger_deinit;

  code = owl_vk_context_device_options_fill (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  code = owl_vk_context_physical_device_select (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  ctx->vk_surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
  ctx->vk_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  code = owl_vk_context_surface_format_ensure (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  ctx->msaa = VK_SAMPLE_COUNT_2_BIT;
  code = owl_vk_context_msaa_ensure (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  ctx->vk_present_mode = VK_PRESENT_MODE_FIFO_KHR;
  code = owl_vk_context_present_mode_ensure (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  code = owl_vk_context_device_init (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_surface_deinit;

  code = owl_vk_context_depth_stencil_format_ensure (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_device_deinit;

  code = owl_vk_context_main_render_pass_init (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_device_deinit;

  code = owl_vk_context_pools_init (ctx);
  if (OWL_SUCCESS != code)
    goto out_error_main_render_pass_deinit;

  goto out;

out_error_main_render_pass_deinit:
  owl_vk_context_main_render_pass_init (ctx);

out_error_device_deinit:
  owl_vk_context_device_deinit (ctx);

out_error_surface_deinit:
  owl_vk_context_surface_deinit (ctx);

out_error_debug_messenger_deinit:
  owl_vk_context_debug_messenger_deinit (ctx);

out_error_instance_deinit:
  owl_vk_context_instance_deinit (ctx);

out:
  return code;
}

owl_public void
owl_vk_context_deinit (struct owl_vk_context *ctx)
{
  owl_vk_context_pools_deinit (ctx);
  owl_vk_context_main_render_pass_deinit (ctx);
  owl_vk_context_device_deinit (ctx);
  owl_vk_context_surface_deinit (ctx);
  owl_vk_context_debug_messenger_deinit (ctx);
  owl_vk_context_instance_deinit (ctx);
}

owl_public owl_u32
owl_vk_context_get_memory_type (struct owl_vk_context const *ctx,
                                owl_u32 filter,
                                enum owl_memory_properties props)
{
  owl_i32 i;
  VkMemoryPropertyFlags flags;
  VkPhysicalDeviceMemoryProperties properties;

  switch (props) {
  case OWL_MEMORY_PROPERTIES_CPU_ONLY:
    flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    break;
  case OWL_MEMORY_PROPERTIES_GPU_ONLY:
    flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  }

  vkGetPhysicalDeviceMemoryProperties (ctx->vk_physical_device, &properties);

  for (i = 0; i < (owl_i32)properties.memoryTypeCount; ++i) {
    owl_u32 current = properties.memoryTypes[i].propertyFlags;

    if ((current & flags) && (filter & (1U << i))) {
      return i;
    }
  }

  return (owl_u32)-1;
}

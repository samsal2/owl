
#include "owl_internal.h"
#include "owl_io.h"
#include "owl_model.h"
#include "owl_vector_math.h"
#include "owl_window.h"
#include "stb_image.h"
#include "stb_truetype.h"

#include <math.h>

#define OWL_MAX_PRESENT_MODES       16
#define OWL_UNRESTRICTED_DIMENSION  (owl_u32) - 1
#define OWL_ACQUIRE_IMAGE_TIMEOUT   (owl_u64) - 1
#define OWL_WAIT_FOR_FENCES_TIMEOUT (owl_u64) - 1
#define OWL_QUEUE_FAMILY_INDEX_NONE (owl_u32) - 1
#define OWL_ALL_SAMPLE_FLAG_BITS    (owl_u32) - 1
#define OWL_JUST_ENOUGH_DESCRIPTORS 256

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

owl_private char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(OWL_ENABLE_VALIDATION)

owl_private char const *const debug_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"};

#endif /* OWL_ENABLE_VALIDATION */

#if !defined(NDEBUG)

#define OWL_VK_CHECK(e)                                                       \
  do {                                                                        \
    VkResult const result_ = e;                                               \
    if (VK_SUCCESS != result_) {                                              \
      OWL_DEBUG_LOG ("OWL_VK_CHECK(%s) result = %i\n", #e, result_);          \
      owl_assert (0);                                                         \
    }                                                                         \
  } while (0)

#else /* NDEBUG */

#define OWL_VK_CHECK(e) e

#endif /* NDEBUG */

owl_private enum owl_code
owl_vk_result_as_owl_code (VkResult vk_result)
{
  switch ((int)vk_result) {
  case VK_SUCCESS:
    return OWL_SUCCESS;

  default:
    return OWL_ERROR_UNKNOWN;
  }
}

owl_private void
owl_renderer_projection_update (struct owl_renderer *r)
{
  float fov = owl_deg2rad (45.0F);
  float ratio = r->framebuffer_ratio;
  float near = 0.01;
  float far = 10.0F;

  owl_m4_perspective (fov, ratio, near, far, r->camera_projection);
}

owl_private void
owl_renderer_view_update (struct owl_renderer *r)
{
  owl_v3 eye = {0.0F, 0.0F, 5.0F};
  owl_v3 direction = {0.0F, 0.0F, 1.0F};
  owl_v3 up = {0.0F, 1.0F, 0.0F};

  owl_m4_look (eye, direction, up, r->camera_view);
}

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

owl_private enum owl_code
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

owl_private void
owl_vk_context_deinit (struct owl_vk_context *ctx)
{
  owl_vk_context_pools_deinit (ctx);
  owl_vk_context_main_render_pass_deinit (ctx);
  owl_vk_context_device_deinit (ctx);
  owl_vk_context_surface_deinit (ctx);
  owl_vk_context_debug_messenger_deinit (ctx);
  owl_vk_context_instance_deinit (ctx);
}

owl_private VkFormat
owl_vk_attachment_type_get_format (enum owl_vk_attachment_type type,
                                   struct owl_vk_context const *ctx)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return ctx->vk_surface_format.format;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return ctx->vk_depth_stencil_format;
  }
}

owl_private VkImageUsageFlagBits
owl_vk_attachment_type_get_usage (enum owl_vk_attachment_type type)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
}

owl_private VkImageAspectFlagBits
owl_vk_attachment_type_get_aspect (enum owl_vk_attachment_type type)
{
  switch (type) {
  case OWL_VK_ATTACHMENT_TYPE_COLOR:
    return VK_IMAGE_ASPECT_COLOR_BIT;

  case OWL_VK_ATTACHMENT_TYPE_DEPTH_STENCIL:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  }
}

owl_private owl_u32
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

  return OWL_RENDERER_MEMORY_TYPE_NONE;
}

owl_private enum owl_code
owl_vk_attachment_init (struct owl_vk_attachment *attachment,
                        struct owl_vk_context *ctx, owl_i32 w, owl_i32 h,
                        enum owl_vk_attachment_type type)
{
  VkImageCreateInfo image_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo memory_info;
  VkImageViewCreateInfo image_view_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.pNext = NULL;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.format = owl_vk_attachment_type_get_format (type, ctx);
  image_info.extent.width = w;
  image_info.extent.height = h;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.samples = ctx->msaa;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.usage = owl_vk_attachment_type_get_usage (type);
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.queueFamilyIndexCount = 0;
  image_info.pQueueFamilyIndices = NULL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_result =
      vkCreateImage (ctx->vk_device, &image_info, NULL, &attachment->vk_image);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkGetImageMemoryRequirements (ctx->vk_device, attachment->vk_image, &req);

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = req.size;
  memory_info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_GPU_ONLY);

  vk_result = vkAllocateMemory (ctx->vk_device, &memory_info, NULL,
                                &attachment->vk_memory);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_image_deinit;
  }

  vk_result = vkBindImageMemory (ctx->vk_device, attachment->vk_image,
                                 attachment->vk_memory, 0);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.pNext = NULL;
  image_view_info.flags = 0;
  image_view_info.image = attachment->vk_image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = image_info.format;
  image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_info.subresourceRange.aspectMask =
      owl_vk_attachment_type_get_aspect (type);
  image_view_info.subresourceRange.baseMipLevel = 0;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.baseArrayLayer = 0;
  image_view_info.subresourceRange.layerCount = 1;

  vk_result = vkCreateImageView (ctx->vk_device, &image_view_info, NULL,
                                 &attachment->vk_image_view);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  goto out;

out_error_memory_deinit:
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);

out_error_image_deinit:
  vkDestroyImage (ctx->vk_device, attachment->vk_image, NULL);

out:
  return code;
}

owl_private void
owl_vk_attachment_deinit (struct owl_vk_attachment *attachment,
                          struct owl_vk_context *ctx)
{
  vkDestroyImageView (ctx->vk_device, attachment->vk_image_view, NULL);
  vkFreeMemory (ctx->vk_device, attachment->vk_memory, NULL);
  vkDestroyImage (ctx->vk_device, attachment->vk_image, NULL);
}

owl_private enum owl_code
owl_vk_frame_heap_init (struct owl_vk_frame_heap *heap,
                        struct owl_vk_context const *ctx,
                        struct owl_vk_pipeline_manager const *pm, owl_u64 sz)
{
  VkBufferCreateInfo buffer_info;
  VkMemoryRequirements req;
  VkMemoryAllocateInfo memory_info;
  VkDescriptorSetAllocateInfo set_info;
  VkDescriptorBufferInfo descriptor;
  VkWriteDescriptorSet write;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  heap->offset = 0;
  heap->size = sz;

  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = NULL;
  buffer_info.flags = 0;
  buffer_info.size = sz;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_info.queueFamilyIndexCount = 0;
  buffer_info.pQueueFamilyIndices = NULL;

  vk_result =
      vkCreateBuffer (ctx->vk_device, &buffer_info, NULL, &heap->vk_buffer);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkGetBufferMemoryRequirements (ctx->vk_device, heap->vk_buffer, &req);

  heap->alignment = req.alignment;

  memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_info.pNext = NULL;
  memory_info.allocationSize = req.size;
  memory_info.memoryTypeIndex = owl_vk_context_get_memory_type (
      ctx, req.memoryTypeBits, OWL_MEMORY_PROPERTIES_CPU_ONLY);

  vk_result =
      vkAllocateMemory (ctx->vk_device, &memory_info, NULL, &heap->vk_memory);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_buffer_deinit;
  }

  vk_result =
      vkBindBufferMemory (ctx->vk_device, heap->vk_buffer, heap->vk_memory, 0);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  vk_result =
      vkMapMemory (ctx->vk_device, heap->vk_memory, 0, sz, 0, &heap->data);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  set_info.pNext = NULL;
  set_info.descriptorPool = ctx->vk_set_pool;
  set_info.descriptorSetCount = 1;
  set_info.pSetLayouts = &pm->vk_vert_ubo_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &set_info,
                                        &heap->vk_vert_ubo_set);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_memory_deinit;
  }

  set_info.pSetLayouts = &pm->vk_both_ubo_set_layout;

  vk_result = vkAllocateDescriptorSets (ctx->vk_device, &set_info,
                                        &heap->vk_both_ubo_set);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_vert_ubo_set_deinit;
  }

  descriptor.buffer = heap->vk_buffer;
  descriptor.offset = 0;
  descriptor.range = sz;

  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = NULL;
  write.dstSet = heap->vk_vert_ubo_set;
  write.dstArrayElement = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  write.pImageInfo = NULL;
  write.pBufferInfo = &descriptor;
  write.pTexelBufferView = NULL;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  write.dstSet = heap->vk_both_ubo_set;

  vkUpdateDescriptorSets (ctx->vk_device, 1, &write, 0, NULL);

  goto out;

out_error_vert_ubo_set_deinit:
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_vert_ubo_set);

out_error_memory_deinit:
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);

out_error_buffer_deinit:
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);

out:
  return code;
}

owl_private void
owl_vk_frame_heap_deinit (struct owl_vk_frame_heap *heap,
                          struct owl_vk_context const *ctx)
{
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_both_ubo_set);
  vkFreeDescriptorSets (ctx->vk_device, ctx->vk_set_pool, 1,
                        &heap->vk_vert_ubo_set);
  vkFreeMemory (ctx->vk_device, heap->vk_memory, NULL);
  vkDestroyBuffer (ctx->vk_device, heap->vk_buffer, NULL);
}

owl_private enum owl_code
owl_vk_frame_sync_init (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context const *ctx)
{
  VkFenceCreateInfo fence_info;
  VkSemaphoreCreateInfo semaphore_info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.pNext = NULL;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vk_result = vkCreateFence (ctx->vk_device, &fence_info, NULL,
                             &sync->vk_in_flight_fence);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_info.pNext = NULL;
  semaphore_info.flags = 0;

  vk_result = vkCreateSemaphore (ctx->vk_device, &semaphore_info, NULL,
                                 &sync->vk_render_done_semaphore);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_in_flight_fence_deinit;
  }

  vk_result = vkCreateSemaphore (ctx->vk_device, &semaphore_info, NULL,
                                 &sync->vk_image_available_semaphore);

  if (VK_SUCCESS != vk_result) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_render_done_semaphore_deinit;
  }

  goto out;

out_error_render_done_semaphore_deinit:
  vkDestroySemaphore (ctx->vk_device, sync->vk_render_done_semaphore, NULL);

out_error_in_flight_fence_deinit:
  vkDestroyFence (ctx->vk_device, sync->vk_in_flight_fence, NULL);

out:
  return code;
}

owl_private void
owl_vk_frame_sync_deinit (struct owl_vk_frame_sync *sync,
                          struct owl_vk_context const *ctx)
{
  vkDestroySemaphore (ctx->vk_device, sync->vk_image_available_semaphore,
                      NULL);
  vkDestroySemaphore (ctx->vk_device, sync->vk_render_done_semaphore, NULL);
  vkDestroyFence (ctx->vk_device, sync->vk_in_flight_fence, NULL);
}

owl_private enum owl_code
owl_vk_frame_sync_wait (struct owl_vk_frame_sync *sync,
                        struct owl_vk_context *ctx)
{
  VkResult vk_result;
  vk_result = vkWaitForFences (ctx->vk_device, 1, &sync->vk_in_flight_fence,
                               VK_TRUE, (owl_u64)-1);
  return VK_SUCCESS == vk_result ? OWL_SUCCESS : OWL_ERROR_UNKNOWN;
}

owl_private enum owl_code
owl_renderer_pools_init (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkCommandPoolCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = r->context.graphics_queue_family;

    vk_result = vkCreateCommandPool (r->context.vk_device, &info, NULL,
                                     &r->transient_command_pool);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
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
    info.maxSets = OWL_JUST_ENOUGH_DESCRIPTORS * owl_array_size (sizes);
    info.poolSizeCount = owl_array_size (sizes);
    info.pPoolSizes = sizes;

    vk_result = vkCreateDescriptorPool (r->context.vk_device, &info, NULL,
                                        &r->common_set_pool);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_transient_command_pool_deinit;
    }
  }

  goto out;

out_transient_command_pool_deinit:
  vkDestroyCommandPool (r->context.vk_device, r->transient_command_pool, NULL);

out:
  return code;
}

owl_private void
owl_renderer_pools_deinit (struct owl_renderer *r)
{
  vkDestroyDescriptorPool (r->context.vk_device, r->common_set_pool, NULL);
  vkDestroyCommandPool (r->context.vk_device, r->transient_command_pool, NULL);
}

owl_private enum owl_code
owl_renderer_set_layouts_init (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
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

    vk_result = vkCreateDescriptorSetLayout (r->context.vk_device, &info, NULL,
                                             &r->vertex_ubo_set_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
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

    vk_result = vkCreateDescriptorSetLayout (r->context.vk_device, &info, NULL,
                                             &r->fragment_ubo_set_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_vertex_ubo_set_layout_deinit;
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

    vk_result = vkCreateDescriptorSetLayout (r->context.vk_device, &info, NULL,
                                             &r->shared_ubo_set_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_fragment_ubo_set_layout_denit;
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

    vk_result = vkCreateDescriptorSetLayout (r->context.vk_device, &info, NULL,
                                             &r->vertex_ssbo_set_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_shared_ubo_set_layout_deinit;
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
    info.bindingCount = owl_array_size (bindings);
    info.pBindings = bindings;

    vk_result = vkCreateDescriptorSetLayout (r->context.vk_device, &info, NULL,
                                             &r->image_set_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_vertex_ssbo_set_layout_denit;
    }
  }

  goto out;

out_error_vertex_ssbo_set_layout_denit:
  vkDestroyDescriptorSetLayout (r->context.vk_device,
                                r->vertex_ssbo_set_layout, NULL);

out_error_shared_ubo_set_layout_deinit:
  vkDestroyDescriptorSetLayout (r->context.vk_device, r->shared_ubo_set_layout,
                                NULL);

out_error_fragment_ubo_set_layout_denit:
  vkDestroyDescriptorSetLayout (r->context.vk_device,
                                r->fragment_ubo_set_layout, NULL);

out_error_vertex_ubo_set_layout_deinit:
  vkDestroyDescriptorSetLayout (r->context.vk_device, r->vertex_ubo_set_layout,
                                NULL);

out:
  return code;
}

owl_private void
owl_renderer_set_layouts_deinit (struct owl_renderer *r)
{
  vkDestroyDescriptorSetLayout (r->context.vk_device, r->image_set_layout,
                                NULL);
  vkDestroyDescriptorSetLayout (r->context.vk_device,
                                r->vertex_ssbo_set_layout, NULL);
  vkDestroyDescriptorSetLayout (r->context.vk_device, r->shared_ubo_set_layout,
                                NULL);
  vkDestroyDescriptorSetLayout (r->context.vk_device,
                                r->fragment_ubo_set_layout, NULL);
  vkDestroyDescriptorSetLayout (r->context.vk_device, r->vertex_ubo_set_layout,
                                NULL);
}

owl_private enum owl_code
owl_renderer_pipeline_layouts_init (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
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
    info.setLayoutCount = owl_array_size (sets);
    info.pSetLayouts = sets;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = NULL;

    vk_result = vkCreatePipelineLayout (r->context.vk_device, &info, NULL,
                                        &r->common_pipeline_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  {
    VkPushConstantRange push_constant;
    VkDescriptorSetLayout sets[6];
    VkPipelineLayoutCreateInfo info;

    push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant.offset = 0;
    push_constant.size = sizeof (struct owl_model_material_push_constant);

    sets[0] = r->shared_ubo_set_layout;
    sets[1] = r->image_set_layout;
    sets[2] = r->image_set_layout;
    sets[3] = r->image_set_layout;
    sets[4] = r->vertex_ssbo_set_layout;
    sets[5] = r->fragment_ubo_set_layout;

    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.setLayoutCount = owl_array_size (sets);
    info.pSetLayouts = sets;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push_constant;

    vk_result = vkCreatePipelineLayout (r->context.vk_device, &info, NULL,
                                        &r->model_pipeline_layout);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_common_pipeline_layout_deinit;
    }
  }

  goto out;

out_error_common_pipeline_layout_deinit:
  vkDestroyPipelineLayout (r->context.vk_device, r->common_pipeline_layout,
                           NULL);

out:
  return code;
}

owl_private void
owl_renderer_pipeline_layouts_deinit (struct owl_renderer *r)
{
  vkDestroyPipelineLayout (r->context.vk_device, r->model_pipeline_layout,
                           NULL);
  vkDestroyPipelineLayout (r->context.vk_device, r->common_pipeline_layout,
                           NULL);
}

owl_private enum owl_code
owl_renderer_shaders_init (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  {
    VkShaderModuleCreateInfo info;

    /* TODO(samuel): Properly load code at runtime */
    owl_local_persist owl_u32 const spv[] = {
#include "owl_glsl_basic.vert.spv.u32"
    };

    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (r->context.vk_device, &info, NULL,
                                      &r->basic_vertex_shader);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
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
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (r->context.vk_device, &info, NULL,
                                      &r->basic_fragment_shader);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_basic_vertex_shader_deinit;
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
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (r->context.vk_device, &info, NULL,
                                      &r->font_fragment_shader);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_basic_fragment_shader_deinit;
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
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (r->context.vk_device, &info, NULL,
                                      &r->model_vertex_shader);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_font_fragment_shader_deinit;
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
    info.codeSize = sizeof (spv);
    info.pCode = spv;

    vk_result = vkCreateShaderModule (r->context.vk_device, &info, NULL,
                                      &r->model_fragment_shader);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_model_vertex_shader_deinit;
    }
  }

  goto out;

out_error_model_vertex_shader_deinit:
  vkDestroyShaderModule (r->context.vk_device, r->model_vertex_shader, NULL);

out_error_font_fragment_shader_deinit:
  vkDestroyShaderModule (r->context.vk_device, r->font_fragment_shader, NULL);

out_error_basic_fragment_shader_deinit:
  vkDestroyShaderModule (r->context.vk_device, r->basic_fragment_shader, NULL);

out_error_basic_vertex_shader_deinit:
  vkDestroyShaderModule (r->context.vk_device, r->basic_vertex_shader, NULL);

out:
  return code;
}

owl_private void
owl_renderer_shaders_deinit (struct owl_renderer *r)
{
  vkDestroyShaderModule (r->context.vk_device, r->model_fragment_shader, NULL);
  vkDestroyShaderModule (r->context.vk_device, r->model_vertex_shader, NULL);
  vkDestroyShaderModule (r->context.vk_device, r->font_fragment_shader, NULL);
  vkDestroyShaderModule (r->context.vk_device, r->basic_fragment_shader, NULL);
  vkDestroyShaderModule (r->context.vk_device, r->basic_vertex_shader, NULL);
}

owl_private enum owl_code
owl_renderer_pipelines_init (struct owl_renderer *r)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
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
      vertex_bindings[0].stride = sizeof (struct owl_renderer_vertex);
      vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attrs[0].binding = 0;
      vertex_attrs[0].location = 0;
      vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[0].offset = offsetof (struct owl_renderer_vertex, position);

      vertex_attrs[1].binding = 0;
      vertex_attrs[1].location = 1;
      vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[1].offset = offsetof (struct owl_renderer_vertex, color);

      vertex_attrs[2].binding = 0;
      vertex_attrs[2].location = 2;
      vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[2].offset = offsetof (struct owl_renderer_vertex, uv);
      break;

    case OWL_RENDERER_PIPELINE_MODEL:
      vertex_bindings[0].binding = 0;
      vertex_bindings[0].stride = sizeof (struct owl_model_vertex);
      vertex_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

      vertex_attrs[0].binding = 0;
      vertex_attrs[0].location = 0;
      vertex_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[0].offset = offsetof (struct owl_model_vertex, position);

      vertex_attrs[1].binding = 0;
      vertex_attrs[1].location = 1;
      vertex_attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
      vertex_attrs[1].offset = offsetof (struct owl_model_vertex, normal);

      vertex_attrs[2].binding = 0;
      vertex_attrs[2].location = 2;
      vertex_attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[2].offset = offsetof (struct owl_model_vertex, uv0);

      vertex_attrs[3].binding = 0;
      vertex_attrs[3].location = 3;
      vertex_attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
      vertex_attrs[3].offset = offsetof (struct owl_model_vertex, uv1);

      vertex_attrs[4].binding = 0;
      vertex_attrs[4].location = 4;
      vertex_attrs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attrs[4].offset = offsetof (struct owl_model_vertex, joints0);

      vertex_attrs[5].binding = 0;
      vertex_attrs[5].location = 5;
      vertex_attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
      vertex_attrs[5].offset = offsetof (struct owl_model_vertex, weights0);
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
      viewport.width = r->swapchain.size.width;
      viewport.height = r->swapchain.size.height;
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
      scissor.extent = r->swapchain.size;
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
      multisample.rasterizationSamples = r->context.msaa;
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
      owl_memset (&depth_stencil.front, 0, sizeof (depth_stencil.front));
      owl_memset (&depth_stencil.back, 0, sizeof (depth_stencil.back));
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
      info.stageCount = owl_array_size (stages);
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
      info.renderPass = r->context.main_render_pass;
      info.subpass = 0;
      info.basePipelineHandle = VK_NULL_HANDLE;
      info.basePipelineIndex = -1;
      break;
    }

    vk_result = vkCreateGraphicsPipelines (r->context.device, VK_NULL_HANDLE,
                                           1, &info, NULL, &r->pipelines[i]);

    if (VK_SUCCESS != vk_result) {
      code = OWL_ERROR_UNKNOWN;
      goto out_error_pipelines_deinit;
    }
  }

  goto out;

out_error_pipelines_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyPipeline (r->context.vk_device, r->pipelines[i], NULL);
  }

out:
  return code;
}

owl_private void
owl_renderer_pipelines_deinit (struct owl_renderer *r)
{
  owl_i32 i;
  for (i = OWL_RENDERER_PIPELINE_COUNT - 1; i >= 0; --i) {
    vkDestroyPipeline (r->context.vk_device, r->pipelines[i], NULL);
  }
}

owl_private enum owl_code
owl_renderer_frame_commands_init (struct owl_renderer *r)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  r->active_frame = 0;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkCommandPoolCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.queueFamilyIndex = r->context.graphics_queue_family;

    vk_result = vkCreateCommandPool (r->context.vk_device, &info, NULL,
                                     &r->frame_command_pools[i]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_command_pools_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkCommandBufferAllocateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = r->frame_command_pools[i];
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers (r->context.vk_device, &info,
                                          &r->frame_command_buffers[i]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_command_buffers_deinit;
    }
  }

  r->active_frame_command_buffer = r->frame_command_buffers[r->active_frame];
  r->active_frame_command_pool = r->frame_command_pools[r->active_frame];

  goto out;

out_error_frame_command_buffers_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkFreeCommandBuffers (r->context.vk_device, r->frame_command_pools[i], 1,
                          &r->frame_command_buffers[i]);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_error_frame_command_pools_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyCommandPool (r->context.vk_device, r->frame_command_pools[i],
                          NULL);
  }

out:
  return code;
}

owl_private void
owl_renderer_frame_commands_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkFreeCommandBuffers (r->context.vk_device, r->frame_command_pools[i], 1,
                          &r->frame_command_buffers[i]);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyCommandPool (r->context.vk_device, r->frame_command_pools[i],
                          NULL);
  }
}

owl_private enum owl_code
owl_renderer_frame_sync_init (struct owl_renderer *r)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkFenceCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_result = vkCreateFence (r->context.vk_device, &info, NULL,
                               &r->in_flight_fences[i]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_in_flight_fences_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkSemaphoreCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vk_result = vkCreateSemaphore (r->context.vk_device, &info, NULL,
                                   &r->render_done_semaphores[i]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_render_done_semaphores_deinit;
    }
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    VkSemaphoreCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;

    vk_result = vkCreateSemaphore (r->context.vk_device, &info, NULL,
                                   &r->image_available_semaphores[i]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_image_available_semaphores_deinit;
    }
  }

  r->active_in_flight_fence = r->in_flight_fences[r->active_frame];

  r->active_render_done_semaphore = r->render_done_semaphores[r->active_frame];

  r->active_image_available_semaphore =
      r->image_available_semaphores[r->active_frame];

  goto out;

out_error_image_available_semaphores_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroySemaphore (r->context.vk_device, r->image_available_semaphores[i],
                        NULL);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_error_render_done_semaphores_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroySemaphore (r->context.vk_device, r->render_done_semaphores[i],
                        NULL);
  }

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_error_in_flight_fences_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyFence (r->context.vk_device, r->in_flight_fences[i], NULL);
  }

out:
  return code;
}

owl_private void
owl_renderer_frame_sync_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyFence (r->context.vk_device, r->in_flight_fences[i], NULL);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroySemaphore (r->context.vk_device, r->render_done_semaphores[i],
                        NULL);
  }

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroySemaphore (r->context.vk_device, r->image_available_semaphores[i],
                        NULL);
  }
}

owl_private enum owl_code
owl_renderer_garbage_init (struct owl_renderer *r)
{
  enum owl_code code = OWL_SUCCESS;

  r->garbage_buffer_count = 0;
  r->garbage_memory_count = 0;
  r->garbage_common_set_count = 0;
  r->garbage_model2_set_count = 0;
  r->garbage_model1_set_count = 0;

  return code;
}

owl_private void
owl_renderer_frame_heap_garbage_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  for (i = 0; i < r->garbage_model1_set_count; ++i) {
    vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                          &r->garbage_model2_sets[i]);
  }

  r->garbage_model1_set_count = 0;

  for (i = 0; i < r->garbage_model2_set_count; ++i) {
    vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                          &r->garbage_model1_sets[i]);
  }

  r->garbage_model2_set_count = 0;

  for (i = 0; i < r->garbage_common_set_count; ++i) {
    vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                          &r->garbage_common_sets[i]);
  }

  r->garbage_common_set_count = 0;

  for (i = 0; i < r->garbage_memory_count; ++i) {
    vkFreeMemory (r->context.vk_device, r->garbage_memories[i], NULL);
  }

  r->garbage_memory_count = 0;

  for (i = 0; i < r->garbage_buffer_count; ++i) {
    vkDestroyBuffer (r->context.vk_device, r->garbage_buffers[i], NULL);
  }

  r->garbage_buffer_count = 0;
}

owl_private enum owl_code
owl_renderer_frame_heap_init (struct owl_renderer *r, owl_u64 sz)
{
  owl_i32 i;

  VkResult vk_result = VK_SUCCESS;
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
      vk_result = vkCreateBuffer (r->context.vk_device, &info, NULL,
                                  &r->frame_heap_buffers[i]);

      code = owl_vk_result_as_owl_code (vk_result);
      if (OWL_SUCCESS != code) {
        goto out_error_frame_heap_buffers_deinit;
      }
    }
  }

  {
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo info;

    vkGetBufferMemoryRequirements (r->context.vk_device,
                                   r->frame_heap_buffers[0], &requirements);

    r->frame_heap_buffer_alignment = requirements.alignment;
    r->frame_heap_buffer_aligned_size =
        owl_alignu2 (sz, requirements.alignment);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize =
        r->frame_heap_buffer_aligned_size * OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;
    info.memoryTypeIndex = owl_renderer_find_memory_type (
        r, requirements.memoryTypeBits,
        OWL_RENDERER_MEMORY_VISIBILITY_CPU_COHERENT);

    vk_result = vkAllocateMemory (r->context.vk_device, &info, NULL,
                                  &r->frame_heap_memory);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_heap_buffers_deinit;
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      vk_result = vkBindBufferMemory (
          r->context.vk_device, r->frame_heap_buffers[i], r->frame_heap_memory,
          i * r->frame_heap_buffer_aligned_size);

      code = owl_vk_result_as_owl_code (vk_result);
      if (OWL_SUCCESS != code) {
        goto out_error_frame_heap_memory_deinit;
      }
    }
  }

  {
    void *data;

    vk_result = vkMapMemory (r->context.vk_device, r->frame_heap_memory, 0,
                             VK_WHOLE_SIZE, 0, &data);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_heap_memory_deinit;
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

    vk_result = vkAllocateDescriptorSets (r->context.vk_device, &info,
                                          r->frame_heap_common_sets);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_heap_memory_unmap;
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

    vk_result = vkAllocateDescriptorSets (r->context.vk_device, &info,
                                          r->frame_heap_model1_sets);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_heap_common_ubo_sets_deinit;
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

    vk_result = vkAllocateDescriptorSets (r->context.vk_device, &info,
                                          r->frame_heap_model2_sets);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_frame_heap_model_ubo_sets_deinit;
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buffer;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof (struct owl_renderer_common_ubo);

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

      vkUpdateDescriptorSets (r->context.vk_device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof (struct owl_model1_ubo);

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

      vkUpdateDescriptorSets (r->context.vk_device, 1, &write, 0, NULL);
    }
  }

  {
    for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
      VkDescriptorBufferInfo buffer;
      VkWriteDescriptorSet write;

      buffer.buffer = r->frame_heap_buffers[i];
      buffer.offset = 0;
      buffer.range = sizeof (struct owl_model2_ubo);

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

      vkUpdateDescriptorSets (r->context.vk_device, 1, &write, 0, NULL);
    }
  }

  r->active_frame_heap_data = r->frame_heap_data[r->active_frame];

  r->active_frame_heap_buffer = r->frame_heap_buffers[r->active_frame];

  r->active_frame_heap_common_set = r->frame_heap_common_sets[r->active_frame];

  r->active_frame_heap_model1_set = r->frame_heap_model1_sets[r->active_frame];

  r->active_frame_heap_model2_set = r->frame_heap_model2_sets[r->active_frame];

  goto out;

out_error_frame_heap_model_ubo_sets_deinit:
  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool,
                        OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                        r->frame_heap_common_sets);

out_error_frame_heap_common_ubo_sets_deinit:
  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool,
                        OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                        r->frame_heap_common_sets);

out_error_frame_heap_memory_unmap:
  vkUnmapMemory (r->context.vk_device, r->frame_heap_memory);

out_error_frame_heap_memory_deinit:
  vkFreeMemory (r->context.vk_device, r->frame_heap_memory, NULL);

  i = OWL_RENDERER_IN_FLIGHT_FRAME_COUNT;

out_error_frame_heap_buffers_deinit:
  for (i = i - 1; i >= 0; --i) {
    vkDestroyBuffer (r->context.vk_device, r->frame_heap_buffers[i], NULL);
  }

out:
  return code;
}

owl_private void
owl_renderer_frame_heap_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool,
                        OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                        r->frame_heap_model2_sets);

  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool,
                        OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                        r->frame_heap_model1_sets);

  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool,
                        OWL_RENDERER_IN_FLIGHT_FRAME_COUNT,
                        r->frame_heap_common_sets);

  vkFreeMemory (r->context.vk_device, r->frame_heap_memory, NULL);

  for (i = 0; i < OWL_RENDERER_IN_FLIGHT_FRAME_COUNT; ++i) {
    vkDestroyBuffer (r->context.vk_device, r->frame_heap_buffers[i], NULL);
  }
}

owl_private enum owl_code
owl_renderer_frame_heap_move_to_garbage (struct owl_renderer *r)
{
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

owl_private enum owl_code
owl_renderer_image_pool_init (struct owl_renderer *r)
{
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    r->image_pool_slots[i] = 0;
  }

  return code;
}

owl_private void
owl_renderer_image_pool_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    if (!r->image_pool_slots[i]) {
      continue;
    }

    r->image_pool_slots[i] = 0;

    vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                          &r->image_pool_sets[i]);

    vkDestroySampler (r->context.vk_device, r->image_pool_samplers[i], NULL);

    vkDestroyImageView (r->context.vk_device, r->image_pool_image_views[i],
                        NULL);

    vkFreeMemory (r->context.vk_device, r->image_pool_memories[i], NULL);

    vkDestroyImage (r->context.vk_device, r->image_pool_images[i], NULL);
  }
}

owl_private enum owl_code
owl_renderer_frame_heap_reserve (struct owl_renderer *r, owl_u64 sz)
{
  enum owl_code code = OWL_SUCCESS;

  /* if the current buffer size is enough do nothing */
  owl_u64 required = r->frame_heap_offsets[r->active_frame] + sz;
  if (required < r->frame_heap_buffer_size) {
    goto out;
  }

  /* unmap the memory as it's still going to be kept alive */
  vkUnmapMemory (r->context.vk_device, r->frame_heap_memory);

  /* move the current frame heap resources to the garbage */
  code = owl_renderer_frame_heap_move_to_garbage (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  /* create a new frame heap with double the required size*/
  code = owl_renderer_frame_heap_init (r, 2 * required);
  if (OWL_SUCCESS != code) {
    goto out;
  }

out:
  return code;
}

owl_public enum owl_code
owl_renderer_swapchain_resize (struct owl_renderer *r,
                               struct owl_window const *w)
{
  enum owl_code code = OWL_SUCCESS;

  /* set the current window and framebuffer dims */
  owl_window_get_window_size (w, &r->window_width, &r->window_height);
  owl_window_get_framebuffer_size (w, &r->framebuffer_width,
                                   &r->framebuffer_height);

  r->framebuffer_ratio = (float)r->window_width / (float)r->window_height;

  owl_renderer_projection_update (r);
  owl_renderer_view_update (r);

  /* make sure no resources are in use */
  OWL_VK_CHECK (vkDeviceWaitIdle (r->context.vk_device));

  owl_vk_attachment_deinit (&r->color_attachment, &r->context);
  owl_vk_attachment_deinit (&r->depth_attachment, &r->context);
  owl_vk_swapchain_deinit (&r->swapchain, &r->context);
  owl_renderer_pipelines_deinit (r);

out:
  return code;
}

owl_public owl_b32
owl_renderer_frame_heap_offset_is_clear (struct owl_renderer const *r)
{
  return 0 == r->frame_heap_offsets[r->active_frame];
}

owl_private void
owl_renderer_frame_heap_garbage_clear (struct owl_renderer *r)
{
  owl_renderer_frame_heap_garbage_deinit (r);
}

owl_public void
owl_renderer_frame_heap_offset_clear (struct owl_renderer *r)
{
  r->frame_heap_offsets[r->active_frame] = 0;
}

owl_public void *
owl_renderer_frame_heap_allocate (
    struct owl_renderer *r, owl_u64 sz,
    struct owl_renderer_frame_heap_reference *ref)
{
  owl_u64 offset;

  owl_byte *data = NULL;
  enum owl_code code = OWL_SUCCESS;

  /* make sure the current heap size is enought */
  code = owl_renderer_frame_heap_reserve (r, sz);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  offset = r->frame_heap_offsets[r->active_frame];

  ref->offset32 = (owl_u32)offset;
  ref->offset = offset;
  ref->buffer = r->active_frame_heap_buffer;
  ref->common_ubo_set = r->active_frame_heap_common_set;
  ref->model1_ubo_set = r->active_frame_heap_model1_set;
  ref->model2_ubo_set = r->active_frame_heap_model2_set;

  data = &r->active_frame_heap_data[offset];

  offset = owl_alignu2 (offset + sz, r->frame_heap_buffer_alignment);
  r->frame_heap_offsets[r->active_frame] = offset;

out:
  return data;
}

owl_public enum owl_code
owl_renderer_frame_heap_submit (struct owl_renderer *r, owl_u64 sz,
                                void const *src,
                                struct owl_renderer_frame_heap_reference *ref)
{
  owl_byte *data;

  enum owl_code code = OWL_SUCCESS;

  data = owl_renderer_frame_heap_allocate (r, sz, ref);
  if (!data) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out;
  }

  owl_memcpy (data, src, sz);

out:
  return code;
}

owl_public owl_u32
owl_renderer_find_memory_type (struct owl_renderer const *r, owl_u32 filter,
                               enum owl_renderer_memory_visibility vis)
{
  owl_i32 i;
  VkMemoryPropertyFlags visibility;
  VkPhysicalDeviceMemoryProperties properties;

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

  vkGetPhysicalDeviceMemoryProperties (r->context.vk_physical_device,
                                       &properties);

  for (i = 0; i < (owl_i32)properties.memoryTypeCount; ++i) {
    owl_u32 flags = properties.memoryTypes[i].propertyFlags;

    if ((flags & visibility) && (filter & (1U << i))) {
      return i;
    }
  }

  return OWL_RENDERER_MEMORY_TYPE_NONE;
}

owl_public enum owl_code
owl_renderer_pipeline_bind (struct owl_renderer *r,
                            enum owl_renderer_pipeline pipeline)
{
  enum owl_code code = OWL_SUCCESS;

  if (OWL_RENDERER_PIPELINE_NONE == pipeline) {
    goto out;
  }

  r->active_pipeline = r->pipelines[pipeline];
  r->active_pipeline_layout = r->pipeline_layouts[pipeline];

  vkCmdBindPipeline (r->active_frame_command_buffer,
                     VK_PIPELINE_BIND_POINT_GRAPHICS, r->active_pipeline);

out:
  return code;
}

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_init (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (!r->immidiate_command_buffer);

  {
    VkCommandBufferAllocateInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = NULL;
    info.commandPool = r->transient_command_pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    vk_result = vkAllocateCommandBuffers (r->context.vk_device, &info,
                                          &r->immidiate_command_buffer);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_begin (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (r->immidiate_command_buffer);

  {
    VkCommandBufferBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    vk_result = vkBeginCommandBuffer (r->immidiate_command_buffer, &info);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_end (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (r->immidiate_command_buffer);

  vk_result = vkEndCommandBuffer (r->immidiate_command_buffer);

  code = owl_vk_result_as_owl_code (vk_result);
  if (OWL_SUCCESS != code) {
    goto out;
  }

out:
  return code;
}

owl_public enum owl_code
owl_renderer_immidiate_command_buffer_submit (struct owl_renderer *r)
{
  VkSubmitInfo info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (r->immidiate_command_buffer);

  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.pNext = NULL;
  info.waitSemaphoreCount = 0;
  info.pWaitSemaphores = NULL;
  info.pWaitDstStageMask = NULL;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &r->immidiate_command_buffer;
  info.signalSemaphoreCount = 0;
  info.pSignalSemaphores = NULL;

  vk_result = vkQueueSubmit (r->graphics_queue, 1, &info, NULL);

  code = owl_vk_result_as_owl_code (vk_result);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  vk_result = vkQueueWaitIdle (r->graphics_queue);

  code = owl_vk_result_as_owl_code (vk_result);
  if (OWL_SUCCESS != code) {
    goto out;
  }

out:
  return code;
}

owl_public void
owl_renderer_immidiate_command_buffer_deinit (struct owl_renderer *r)
{
  owl_assert (r->immidiate_command_buffer);

  vkFreeCommandBuffers (r->context.vk_device, r->transient_command_pool, 1,
                        &r->immidiate_command_buffer);

  r->immidiate_command_buffer = VK_NULL_HANDLE;
}

owl_private enum owl_code
owl_renderer_swapchain_acquire_next_image (struct owl_renderer *r)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  vk_result = vkAcquireNextImageKHR (
      r->context.vk_device, r->swapchain, OWL_ACQUIRE_IMAGE_TIMEOUT,
      r->active_image_available_semaphore, VK_NULL_HANDLE,
      &r->active_swapchain_image_index);

  if (VK_ERROR_OUT_OF_DATE_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  }

  r->active_swapchain_image =
      r->swapchain_images[r->active_swapchain_image_index];

  r->active_swapchain_framebuffer =
      r->swapchain_framebuffers[r->active_swapchain_image_index];

  return code;
}

owl_private void
owl_renderer_prepare_frame (struct owl_renderer *r)
{
  OWL_VK_CHECK (vkWaitForFences (r->context.vk_device, 1,
                                 &r->active_in_flight_fence, VK_TRUE,
                                 OWL_WAIT_FOR_FENCES_TIMEOUT));

  OWL_VK_CHECK (
      vkResetFences (r->context.vk_device, 1, &r->active_in_flight_fence));

  OWL_VK_CHECK (vkResetCommandPool (r->context.vk_device,
                                    r->active_frame_command_pool, 0));
}

owl_private void
owl_renderer_begin_recording (struct owl_renderer *r)
{
  {
    VkCommandBufferBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.pNext = NULL;
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    info.pInheritanceInfo = NULL;

    OWL_VK_CHECK (
        vkBeginCommandBuffer (r->active_frame_command_buffer, &info));
  }

  {
    VkRenderPassBeginInfo info;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.pNext = NULL;
    info.renderPass = r->context.vk_main_render_pass;
    info.framebuffer = r->active_swapchain_framebuffer;
    info.renderArea.offset.x = 0;
    info.renderArea.offset.y = 0;
    info.renderArea.extent = r->swapchain.size;
    info.clearValueCount = owl_array_size (r->swapchain_clear_values);
    info.pClearValues = r->swapchain_clear_values;

    vkCmdBeginRenderPass (r->active_frame_command_buffer, &info,
                          VK_SUBPASS_CONTENTS_INLINE);
  }
}

owl_public enum owl_code
owl_renderer_frame_begin (struct owl_renderer *r)
{
  enum owl_code code = OWL_SUCCESS;

  code = owl_renderer_swapchain_acquire_next_image (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  owl_renderer_prepare_frame (r);
  owl_renderer_begin_recording (r);

out:
  return code;
}

owl_private void
owl_renderer_end_recording (struct owl_renderer *r)
{
  vkCmdEndRenderPass (r->active_frame_command_buffer);
  OWL_VK_CHECK (vkEndCommandBuffer (r->active_frame_command_buffer));
}

owl_private void
owl_renderer_submit_graphics (struct owl_renderer *r)
{
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

  OWL_VK_CHECK (
      vkQueueSubmit (r->graphics_queue, 1, &info, r->active_in_flight_fence));
}

owl_private enum owl_code
owl_renderer_present_swapchain (struct owl_renderer *r)
{
  VkPresentInfoKHR info;

  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;

  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = NULL;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &r->active_render_done_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &r->swapchain;
  info.pImageIndices = &r->active_swapchain_image_index;
  info.pResults = NULL;

  vk_result = vkQueuePresentKHR (r->present_queue, &info);

  if (VK_ERROR_OUT_OF_DATE_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_SUBOPTIMAL_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  } else if (VK_ERROR_SURFACE_LOST_KHR == vk_result) {
    code = OWL_ERROR_OUTDATED_SWAPCHAIN;
  }

  return code;
}

owl_private void
owl_renderer_actives_update (struct owl_renderer *r)
{
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

owl_private void
owl_renderer_time_stamps_update (struct owl_renderer *r)
{
  r->time_stamp_previous = r->time_stamp_current;
  r->time_stamp_current = owl_io_time_stamp_get ();
  r->time_stamp_delta = r->time_stamp_current - r->time_stamp_previous;
}

owl_public enum owl_code
owl_renderer_frame_end (struct owl_renderer *r)
{
  enum owl_code code = OWL_SUCCESS;

  owl_renderer_end_recording (r);
  owl_renderer_submit_graphics (r);

  code = owl_renderer_present_swapchain (r);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  owl_renderer_actives_update (r);
  owl_renderer_frame_heap_offset_clear (r);
  owl_renderer_frame_heap_garbage_clear (r);
  owl_renderer_time_stamps_update (r);

out:
  return code;
}

owl_private VkFormat
owl_as_vk_format (enum owl_renderer_pixel_format fmt)
{
  switch (fmt) {
  case OWL_RENDERER_PIXEL_FORMAT_R8_UNORM:
    return VK_FORMAT_R8_UNORM;

  case OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  }
}

owl_private VkFilter
owl_as_vk_filter (enum owl_renderer_sampler_filter filter)
{
  switch (filter) {
  case OWL_RENDERER_SAMPLER_FILTER_NEAREST:
    return VK_FILTER_NEAREST;

  case OWL_RENDERER_SAMPLER_FILTER_LINEAR:
    return VK_FILTER_LINEAR;
  }
}

owl_private VkSamplerMipmapMode
owl_as_vk_sampler_mipmap_mode (enum owl_renderer_sampler_mip_mode mode)
{
  switch (mode) {
  case OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;

  case OWL_RENDERER_SAMPLER_MIP_MODE_LINEAR:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

owl_private VkSamplerAddressMode
owl_as_vk_sampler_addr_mode (enum owl_renderer_sampler_addr_mode mode)
{
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

owl_private owl_u64
owl_renderer_pixel_format_size (enum owl_renderer_pixel_format format)
{
  switch (format) {
  case OWL_RENDERER_PIXEL_FORMAT_R8_UNORM:
    return 1 * sizeof (owl_u8);

  case OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB:
    return 4 * sizeof (owl_u8);
  }
}

owl_private enum owl_code
owl_renderer_image_pool_find_slot (struct owl_renderer *r,
                                   owl_renderer_image_id *img)
{
  owl_i32 i;
  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_IMAGE_POOL_SLOT_COUNT; ++i) {
    /* skip all active slots */
    if (r->image_pool_slots[i]) {
      continue;
    }

    /* found unactive slot */
    *img = i;
    r->image_pool_slots[i] = 1;

    goto out;
  }

  /* no active slot was found */
  code = OWL_ERROR_UNKNOWN;
  *img = OWL_RENDERER_IMAGE_POOL_SLOT_COUNT;

out:
  return code;
}

owl_private owl_u32
owl_calc_mip_levels (owl_u32 w, owl_u32 h)
{
  return (owl_u32)(floor (log2 (owl_max (w, h))) + 1);
}

owl_private enum owl_code
owl_renderer_image_transition (struct owl_renderer const *r,
                               owl_renderer_image_id img, owl_u32 mips,
                               owl_u32 layers, VkImageLayout src_layout,
                               VkImageLayout dst_layout)
{
  VkImageMemoryBarrier barrier;
  VkPipelineStageFlags src = VK_PIPELINE_STAGE_NONE_KHR;
  VkPipelineStageFlags dst = VK_PIPELINE_STAGE_NONE_KHR;
  enum owl_code code = OWL_SUCCESS;

  owl_assert (r->immidiate_command_buffer);

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask = Defined later */
  /* image_memory_barrier.dstAccessMask = Defined later */
  barrier.oldLayout = src_layout;
  barrier.newLayout = dst_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = r->image_pool_images[img];
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mips;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = layers;

  if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == src_layout &&
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (VK_IMAGE_LAYOUT_UNDEFINED == src_layout &&
             VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  } else {
    owl_assert (0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == dst_layout) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else {
    owl_assert (0 && "Invalid arguments");
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  vkCmdPipelineBarrier (r->immidiate_command_buffer, src, dst, 0, 0, NULL, 0,
                        NULL, 1, &barrier);

out:
  return code;
}

owl_private enum owl_code
owl_renderer_image_generate_mips (struct owl_renderer const *r,
                                  owl_renderer_image_id img, owl_i32 w,
                                  owl_i32 h, owl_i32 mips)
{
  owl_i32 i;
  VkImageMemoryBarrier barrier;

  enum owl_code code = OWL_SUCCESS;

  owl_assert (r->immidiate_command_buffer);

  if (1 == mips || 0 == mips) {
    goto out;
  }

  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.pNext = NULL;
  /* image_memory_barrier.srcAccessMask =  later */
  /* image_memory_barrier.dstAccessMask =  later */
  /* image_memory_barrier.oldLayout =  later */
  /* image_memory_barrier.newLayout =  later */
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = r->image_pool_images[img];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  for (i = 0; i < (owl_i32)(mips - 1); ++i) {
    barrier.subresourceRange.baseMipLevel = i;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    vkCmdPipelineBarrier (
        r->immidiate_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    {
      VkImageBlit blit;

      blit.srcOffsets[0].x = 0;
      blit.srcOffsets[0].y = 0;
      blit.srcOffsets[0].z = 0;
      blit.srcOffsets[1].x = w;
      blit.srcOffsets[1].y = h;
      blit.srcOffsets[1].z = 1;
      blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.srcSubresource.mipLevel = i;
      blit.srcSubresource.baseArrayLayer = 0;
      blit.srcSubresource.layerCount = 1;
      blit.dstOffsets[0].x = 0;
      blit.dstOffsets[0].y = 0;
      blit.dstOffsets[0].z = 0;
      blit.dstOffsets[1].x = (w > 1) ? (w /= 2) : 1;
      blit.dstOffsets[1].y = (h > 1) ? (h /= 2) : 1;
      blit.dstOffsets[1].z = 1;
      blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blit.dstSubresource.mipLevel = i + 1;
      blit.dstSubresource.baseArrayLayer = 0;
      blit.dstSubresource.layerCount = 1;

      vkCmdBlitImage (
          r->immidiate_command_buffer, r->image_pool_images[img],
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, r->image_pool_images[img],
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    }

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier (r->immidiate_command_buffer,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                          NULL, 1, &barrier);
  }

  barrier.subresourceRange.baseMipLevel = mips - 1;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier (
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

owl_private enum owl_code
owl_renderer_image_load_state_init (
    struct owl_renderer *r, struct owl_renderer_image_desc const *info,
    struct owl_renderer_image_load_state *state)
{
  enum owl_code code = OWL_SUCCESS;

  switch (info->src_type) {
  case OWL_RENDERER_IMAGE_SRC_TYPE_FILE: {
    owl_i32 width;
    owl_i32 height;
    owl_i32 channels;
    owl_u64 sz;
    owl_byte *data;

    data =
        stbi_load (info->src_path, &width, &height, &channels, STBI_rgb_alpha);

    if (!data) {
      code = OWL_ERROR_UNKNOWN;
      goto out;
    }

    state->format = OWL_RENDERER_PIXEL_FORMAT_R8G8B8A8_SRGB;
    state->width = (owl_u32)width;
    state->height = (owl_u32)height;
    sz = state->width * state->height *
         owl_renderer_pixel_format_size (state->format);

    code = owl_renderer_frame_heap_submit (r, sz, data, &state->reference);
    stbi_image_free (data);
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
         owl_renderer_pixel_format_size (state->format);

    code = owl_renderer_frame_heap_submit (r, sz, info->src_data,
                                           &state->reference);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  } break;
  }

  state->mips = owl_calc_mip_levels (state->width, state->height);

out:
  return code;
}

owl_private void
owl_renderer_image_load_state_deinit (
    struct owl_renderer *r, struct owl_renderer_image_load_state *state)
{
  owl_unused (state);
  owl_renderer_frame_heap_offset_clear (r);
}

owl_public enum owl_code
owl_renderer_image_init (struct owl_renderer *r,
                         struct owl_renderer_image_desc const *desc,
                         owl_renderer_image_id *img)
{
  VkResult vk_result = VK_SUCCESS;
  enum owl_code code = OWL_SUCCESS;
  struct owl_renderer_image_load_state state;

  owl_assert (owl_renderer_frame_heap_offset_is_clear (r));

  code = owl_renderer_image_pool_find_slot (r, img);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_image_load_state_init (r, desc, &state);
  if (OWL_SUCCESS != code) {
    goto out_error_image_pool_slot_unselect;
  }

  {
    VkImageCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = owl_as_vk_format (state.format);
    info.extent.width = state.width;
    info.extent.height = state.height;
    info.extent.depth = 1;
    info.mipLevels = state.mips;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vk_result = vkCreateImage (r->context.vk_device, &info, NULL,
                               &r->image_pool_images[*img]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_image_load_state_deinit;
    }
  }

  {
    VkMemoryRequirements req;
    VkMemoryAllocateInfo info;

    vkGetImageMemoryRequirements (r->context.vk_device,
                                  r->image_pool_images[*img], &req);

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext = NULL;
    info.allocationSize = req.size;
    info.memoryTypeIndex = owl_renderer_find_memory_type (
        r, req.memoryTypeBits, OWL_RENDERER_MEMORY_VISIBILITY_GPU);

    vk_result = vkAllocateMemory (r->context.vk_device, &info, NULL,
                                  &r->image_pool_memories[*img]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_image_deinit;
    }

    vk_result =
        vkBindImageMemory (r->context.vk_device, r->image_pool_images[*img],
                           r->image_pool_memories[*img], 0);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_memory_deinit;
    }
  }

  {
    VkImageViewCreateInfo info;

    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.image = r->image_pool_images[*img];
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = owl_as_vk_format (state.format);
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = state.mips;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;

    vk_result = vkCreateImageView (r->context.vk_device, &info, NULL,
                                   &r->image_pool_image_views[*img]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_memory_deinit;
    }
  }

  {
    VkDescriptorSetLayout layout;
    VkDescriptorSetAllocateInfo info;

    layout = r->image_set_layout;

    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pNext = NULL;
    info.descriptorPool = r->common_set_pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    vk_result = vkAllocateDescriptorSets (r->context.vk_device, &info,
                                          &r->image_pool_sets[*img]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_image_view_deinit;
    }
  }

  code = owl_renderer_immidiate_command_buffer_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_set_deinit;
  }

  code = owl_renderer_immidiate_command_buffer_begin (r);
  if (OWL_SUCCESS != code) {
    goto out_error_immidiate_command_buffer_deinit;
  }

  code = owl_renderer_image_transition (r, *img, state.mips, 1,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  if (OWL_SUCCESS != code) {
    goto out_error_immidiate_command_buffer_deinit;
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

    vkCmdCopyBufferToImage (r->immidiate_command_buffer,
                            state.reference.buffer, r->image_pool_images[*img],
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
  }

  code = owl_renderer_image_generate_mips (r, *img, state.width, state.height,
                                           state.mips);
  if (OWL_SUCCESS != code) {
    goto out_error_immidiate_command_buffer_deinit;
  }

  code = owl_renderer_immidiate_command_buffer_end (r);
  if (OWL_SUCCESS != code) {
    goto out_error_immidiate_command_buffer_deinit;
  }

  /* NOTE(samuel): if this fails, immidiate_command_buffer shouldn't be in a
   * pending state, right????? */
  code = owl_renderer_immidiate_command_buffer_submit (r);
  if (OWL_SUCCESS != code) {
    goto out_error_immidiate_command_buffer_deinit;
  }

  owl_renderer_immidiate_command_buffer_deinit (r);

  {
    VkSamplerCreateInfo info;

    if (desc->sampler_use_default) {
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
      info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      info.unnormalizedCoordinates = VK_FALSE;
    } else {
      info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      info.pNext = NULL;
      info.flags = 0;
      info.magFilter = owl_as_vk_filter (desc->sampler_mag_filter);
      info.minFilter = owl_as_vk_filter (desc->sampler_min_filter);
      info.mipmapMode = owl_as_vk_sampler_mipmap_mode (desc->sampler_mip_mode);
      info.addressModeU = owl_as_vk_sampler_addr_mode (desc->sampler_wrap_u);
      info.addressModeV = owl_as_vk_sampler_addr_mode (desc->sampler_wrap_v);
      info.addressModeW = owl_as_vk_sampler_addr_mode (desc->sampler_wrap_w);
      info.mipLodBias = 0.0F;
      info.anisotropyEnable = VK_TRUE;
      info.maxAnisotropy = 16.0F;
      info.compareEnable = VK_FALSE;
      info.compareOp = VK_COMPARE_OP_ALWAYS;
      info.minLod = 0.0F;
      info.maxLod = state.mips; /* VK_LOD_CLAMP_NONE */
      info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
      info.unnormalizedCoordinates = VK_FALSE;
    }

    vk_result = vkCreateSampler (r->context.vk_device, &info, NULL,
                                 &r->image_pool_samplers[*img]);

    code = owl_vk_result_as_owl_code (vk_result);
    if (OWL_SUCCESS != code) {
      goto out_error_immidiate_command_buffer_deinit;
    }
  }

  {
    VkDescriptorImageInfo image;
    VkDescriptorImageInfo sampler;
    VkWriteDescriptorSet writes[2];

    sampler.sampler = r->image_pool_samplers[*img];
    sampler.imageView = VK_NULL_HANDLE;
    sampler.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    image.sampler = VK_NULL_HANDLE;
    image.imageView = r->image_pool_image_views[*img];
    image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = NULL;
    writes[0].dstSet = r->image_pool_sets[*img];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[0].pImageInfo = &sampler;
    writes[0].pBufferInfo = NULL;
    writes[0].pTexelBufferView = NULL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].pNext = NULL;
    writes[1].dstSet = r->image_pool_sets[*img];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[1].pImageInfo = &image;
    writes[1].pBufferInfo = NULL;
    writes[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets (r->context.vk_device, owl_array_size (writes),
                            writes, 0, NULL);
  }

  owl_renderer_image_load_state_deinit (r, &state);

  goto out;

out_error_immidiate_command_buffer_deinit:
  owl_renderer_immidiate_command_buffer_deinit (r);

out_error_set_deinit:
  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                        &r->image_pool_sets[*img]);

out_error_image_view_deinit:
  vkDestroyImageView (r->context.vk_device, r->image_pool_image_views[*img],
                      NULL);

out_error_memory_deinit:
  vkFreeMemory (r->context.vk_device, r->image_pool_memories[*img], NULL);

out_error_image_deinit:
  vkDestroyImage (r->context.vk_device, r->image_pool_images[*img], NULL);

out_error_image_load_state_deinit:
  owl_renderer_image_load_state_deinit (r, &state);

out_error_image_pool_slot_unselect:
  r->image_pool_slots[*img] = 0;

out:
  return code;
}

owl_public void
owl_renderer_image_deinit (struct owl_renderer *r, owl_renderer_image_id img)
{
  OWL_VK_CHECK (vkDeviceWaitIdle (r->context.vk_device));
  owl_assert (r->image_pool_slots[img]);

  r->image_pool_slots[img] = 0;

  vkFreeDescriptorSets (r->context.vk_device, r->common_set_pool, 1,
                        &r->image_pool_sets[img]);

  vkDestroySampler (r->context.vk_device, r->image_pool_samplers[img], NULL);
  vkDestroyImageView (r->context.vk_device, r->image_pool_image_views[img],
                      NULL);
  vkFreeMemory (r->context.vk_device, r->image_pool_memories[img], NULL);
  vkDestroyImage (r->context.vk_device, r->image_pool_images[img], NULL);
}

owl_public void
owl_renderer_active_font_set (struct owl_renderer *r, owl_renderer_font_id fd)
{
  r->active_font = fd;
}

owl_private enum owl_code
owl_renderer_font_load_file (char const *path, owl_byte **data)
{
  owl_u64 sz;
  FILE *file;

  enum owl_code code = OWL_SUCCESS;

  file = fopen (path, "rb");
  if (!file) {
    code = OWL_ERROR_UNKNOWN;
    goto out;
  }

  fseek (file, 0, SEEK_END);

  sz = ftell (file);

  fseek (file, 0, SEEK_SET);

  *data = owl_malloc (sz);
  if (!*data) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out_file_close;
  }

  fread (*data, sz, 1, file);

out_file_close:
  fclose (file);

out:
  return code;
}

owl_private void
owl_renderer_font_file_unload (owl_byte *data)
{
  owl_free (data);
}

owl_private enum owl_code
owl_renderer_font_pool_find_slot (struct owl_renderer *r,
                                  owl_renderer_font_id *fd)
{
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

#define OWL_RENDERER_FONT_ATLAS_WIDTH  1024
#define OWL_RENDERER_FONT_ATLAS_HEIGHT 1024
#define OWL_RENDERER_FONT_FIRST_CHAR   ((owl_i32)(' '))
#define OWL_RENDERER_FONT_CHAR_COUNT   ((owl_i32)('~' - ' '))
#define OWL_RENDERER_FONT_ATLAS_SIZE                                          \
  (OWL_RENDERER_FONT_ATLAS_WIDTH * OWL_RENDERER_FONT_ATLAS_HEIGHT)

owl_static_assert (
    sizeof (struct owl_packed_glyph) == sizeof (stbtt_packedchar),
    "owl_font_char and stbtt_packedchar must represent the same struct");

owl_public enum owl_code
owl_renderer_font_init (struct owl_renderer *r, char const *path, owl_i32 sz,
                        owl_renderer_font_id *font)
{
  owl_b32 pack;
  owl_byte *file;
  owl_byte *bitmap;
  stbtt_pack_context pack_context;
  struct owl_renderer_image_desc desc;

  enum owl_code code = OWL_SUCCESS;

  code = owl_renderer_font_pool_find_slot (r, font);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_font_load_file (path, &file);
  if (OWL_SUCCESS != code) {
    goto out_error_font_slot_unselect;
  }

  bitmap = owl_calloc (OWL_RENDERER_FONT_ATLAS_SIZE, sizeof (owl_byte));
  if (!bitmap) {
    code = OWL_ERROR_BAD_ALLOCATION;
    goto out_error_font_file_unload;
  }

  pack = stbtt_PackBegin (&pack_context, bitmap, OWL_RENDERER_FONT_ATLAS_WIDTH,
                          OWL_RENDERER_FONT_ATLAS_HEIGHT, 0, 1, NULL);
  if (!pack) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_bitmap_free;
  }

  /* FIXME(samuel): hardcoded */
  stbtt_PackSetOversampling (&pack_context, 2, 2);

  /* HACK(samuel): idk if it's legal to alias a different type with the exact
   * same layout, but it _works_ so ill leave it at that*/
  pack = stbtt_PackFontRange (
      &pack_context, file, 0, sz, OWL_RENDERER_FONT_FIRST_CHAR,
      OWL_RENDERER_FONT_CHAR_COUNT,
      (stbtt_packedchar *)(&r->font_pool_packed_glyphs[*font][0]));
  if (!pack) {
    code = OWL_ERROR_UNKNOWN;
    goto out_error_pack_end;
  }

  desc.src_type = OWL_RENDERER_IMAGE_SRC_TYPE_DATA;
  desc.src_path = NULL;
  desc.src_data = bitmap;
  desc.src_data_width = OWL_RENDERER_FONT_ATLAS_WIDTH;
  desc.src_data_height = OWL_RENDERER_FONT_ATLAS_HEIGHT;
  desc.src_data_pixel_format = OWL_RENDERER_PIXEL_FORMAT_R8_UNORM;
  desc.sampler_use_default = 0;
  desc.sampler_mip_mode = OWL_RENDERER_SAMPLER_MIP_MODE_NEAREST;
  desc.sampler_min_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  desc.sampler_mag_filter = OWL_RENDERER_SAMPLER_FILTER_NEAREST;
  desc.sampler_wrap_u = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  desc.sampler_wrap_v = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;
  desc.sampler_wrap_w = OWL_RENDERER_SAMPLER_ADDR_MODE_CLAMP_TO_EDGE;

  code = owl_renderer_image_init (r, &desc, &r->font_pool_atlases[*font]);
  if (OWL_SUCCESS != code) {
    goto out_error_pack_end;
  }

  stbtt_PackEnd (&pack_context);
  owl_free (bitmap);
  owl_renderer_font_file_unload (file);

  goto out;

out_error_pack_end:
  stbtt_PackEnd (&pack_context);

out_error_bitmap_free:
  owl_free (bitmap);

out_error_font_file_unload:
  owl_renderer_font_file_unload (file);

out_error_font_slot_unselect:
  r->font_pool_slots[*font] = 0;

out:
  return code;
}

owl_public void
owl_renderer_font_deinit (struct owl_renderer *r, owl_renderer_font_id fd)
{
  if (fd == r->active_font) {
    r->active_font = OWL_RENDERER_FONT_NONE;
  }

  r->font_pool_slots[fd] = 0;

  owl_renderer_image_deinit (r, r->font_pool_atlases[fd]);
}

owl_public enum owl_code
owl_renderer_active_font_fill_glyph (struct owl_renderer const *r, char c,
                                     owl_v2 offset, struct owl_glyph *glyph)
{
  return owl_renderer_font_fill_glyph (r, c, offset, r->active_font, glyph);
}

owl_public enum owl_code
owl_renderer_font_fill_glyph (struct owl_renderer const *r, char c,
                              owl_v2 offset, owl_renderer_font_id font,
                              struct owl_glyph *glyph)
{
  stbtt_aligned_quad quad;
  enum owl_code code = OWL_SUCCESS;

  if (OWL_RENDERER_FONT_NONE == font) {
    owl_assert (0);
    code = OWL_ERROR_BAD_HANDLE;
    goto out;
  }

  if (OWL_RENDERER_FONT_POOL_SLOT_COUNT <= font) {
    owl_assert (0);
    code = OWL_ERROR_OUT_OF_BOUNDS;
    goto out;
  }

  stbtt_GetPackedQuad (
      (stbtt_packedchar *)(&r->font_pool_packed_glyphs[font][0]),
      OWL_RENDERER_FONT_ATLAS_WIDTH, OWL_RENDERER_FONT_ATLAS_HEIGHT,
      c - OWL_RENDERER_FONT_FIRST_CHAR, &offset[0], &offset[1], &quad, 1);

  owl_v3_set (glyph->positions[0], quad.x0, quad.y0, 0.0F);
  owl_v3_set (glyph->positions[1], quad.x1, quad.y0, 0.0F);
  owl_v3_set (glyph->positions[2], quad.x0, quad.y1, 0.0F);
  owl_v3_set (glyph->positions[3], quad.x1, quad.y1, 0.0F);

  owl_v2_set (glyph->uvs[0], quad.s0, quad.t0);
  owl_v2_set (glyph->uvs[1], quad.s1, quad.t0);
  owl_v2_set (glyph->uvs[2], quad.s0, quad.t1);
  owl_v2_set (glyph->uvs[3], quad.s1, quad.t1);

out:
  return code;
}

owl_private enum owl_code
owl_renderer_font_pool_init (struct owl_renderer *r)
{
  owl_i32 i;

  enum owl_code code = OWL_SUCCESS;

  for (i = 0; i < OWL_RENDERER_FONT_POOL_SLOT_COUNT; ++i) {
    r->font_pool_slots[i] = 0;
  }

  r->active_font = OWL_RENDERER_FONT_NONE;

  return code;
}

owl_private void
owl_renderer_font_pool_deinit (struct owl_renderer *r)
{
  owl_i32 i;

  r->active_font = OWL_RENDERER_FONT_NONE;

  for (i = 0; i < OWL_RENDERER_FONT_POOL_SLOT_COUNT; ++i) {
    if (!r->font_pool_slots[i]) {
      continue;
    }

    r->font_pool_slots[i] = 0;
    owl_renderer_image_deinit (r, r->font_pool_atlases[i]);
  }
}

owl_public enum owl_code
owl_renderer_init (struct owl_renderer *r, struct owl_window const *w)
{
  owl_i32 width;
  owl_i32 height;
  enum owl_code code = OWL_SUCCESS;

  owl_window_get_framebuffer_size (w, &width, &height);

  code = owl_vk_context_init (&r->context, w);
  if (OWL_SUCCESS != code)
    goto out;

  code = owl_vk_attachment_init (&r->color_attachment, &r->context, width,
                                 height, OWL_VK_ATTACHMENT_TYPE_COLOR);
  if (OWL_SUCCESS != code)
    goto out_error_context_deinit;

  code = owl_vk_attachment_init (&r->depth_stencil_attachment, &r->context,
                                 width, height, OWL_VK_ATTACHMENT_TYPE_DEPTH);
  if (OWL_SUCCESS != code)
    goto out_error_color_attachment_deinit;

  code = owl_vk_swapchain_init (&r->swapchain, *r->context,
                                &r->color_attachment, &r->depth_attachment);
  if (OWL_SUCCESS != code) {
    goto out_error_depth_stenciL_attachment_deinit;
  }

  code = owl_renderer_pools_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_swapchain_image_views_deinit;
  }

  code = owl_renderer_set_layouts_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_swapchain_framebuffers_deinit;
  }

  code = owl_renderer_pipeline_layouts_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_set_layouts_deinit;
  }

  code = owl_renderer_shaders_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_pipeline_layouts_deinit;
  }

  code = owl_renderer_pipelines_init (r);
  if (OWL_SUCCESS != code) {
    goto out_error_shaders_deinit;
  }

out:
  return code;
}

void
owl_renderer_deinit (struct owl_renderer *r)
{
  OWL_VK_CHECK (vkDeviceWaitIdle (r->context.vk_device));

  owl_renderer_font_pool_deinit (r);
  owl_renderer_image_pool_deinit (r);
  owl_renderer_frame_heap_deinit (r);
  owl_renderer_frame_heap_garbage_deinit (r);
  owl_renderer_frame_sync_deinit (r);
  owl_renderer_frame_commands_deinit (r);
  owl_renderer_pipelines_deinit (r);
  owl_renderer_shaders_deinit (r);
  owl_renderer_pipeline_layouts_deinit (r);
  owl_renderer_set_layouts_deinit (r);
  owl_renderer_swapchain_framebuffers_deinit (r);
  owl_renderer_attachments_deinit (r);
  owl_renderer_main_render_pass_deinit (r);
  owl_renderer_pools_deinit (r);
  owl_renderer_swapchain_image_views_deinit (r);
  owl_renderer_swapchain_deinit (r);
  owl_renderer_device_deinit (r);
  owl_vk_context_deinit (&r->context);
}

enum owl_code
owl_renderer_quad_draw (struct owl_renderer *r, owl_m4 const matrix,
                        struct owl_renderer_quad const *quad)
{
  VkDescriptorSet sets[2];
  struct owl_renderer_common_ubo ubo;
  struct owl_renderer_vertex vertices[4];
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  struct owl_renderer_frame_heap_reference uref;

  enum owl_code code = OWL_SUCCESS;
  owl_u32 const indices[] = {2, 3, 1, 1, 0, 2};

  vertices[0].position[0] = quad->position0[0];
  vertices[0].position[1] = quad->position0[1];
  vertices[0].position[2] = 0.0F;
  vertices[0].color[0] = quad->color[0];
  vertices[0].color[1] = quad->color[1];
  vertices[0].color[2] = quad->color[2];
  vertices[0].uv[0] = quad->uv0[0];
  vertices[0].uv[1] = quad->uv0[1];

  vertices[1].position[0] = quad->position1[0];
  vertices[1].position[1] = quad->position0[1];
  vertices[1].position[2] = 0.0F;
  vertices[1].color[0] = quad->color[0];
  vertices[1].color[1] = quad->color[1];
  vertices[1].color[2] = quad->color[2];
  vertices[1].uv[0] = quad->uv1[0];
  vertices[1].uv[1] = quad->uv0[1];

  vertices[2].position[0] = quad->position0[0];
  vertices[2].position[1] = quad->position1[1];
  vertices[2].position[2] = 0.0F;
  vertices[2].color[0] = quad->color[0];
  vertices[2].color[1] = quad->color[1];
  vertices[2].color[2] = quad->color[2];
  vertices[2].uv[0] = quad->uv0[0];
  vertices[2].uv[1] = quad->uv1[1];

  vertices[3].position[0] = quad->position1[0];
  vertices[3].position[1] = quad->position1[1];
  vertices[3].position[2] = 0.0F;
  vertices[3].color[0] = quad->color[0];
  vertices[3].color[1] = quad->color[1];
  vertices[3].color[2] = quad->color[2];
  vertices[3].uv[0] = quad->uv1[0];
  vertices[3].uv[1] = quad->uv1[1];

  code =
      owl_renderer_frame_heap_submit (r, sizeof (vertices), vertices, &vref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  code = owl_renderer_frame_heap_submit (r, sizeof (indices), indices, &iref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  owl_m4_copy (r->camera_projection, ubo.projection);
  owl_m4_copy (r->camera_view, ubo.view);
  owl_m4_copy (matrix, ubo.model);

  code = owl_renderer_frame_heap_submit (r, sizeof (ubo), &ubo, &uref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  sets[0] = uref.common_ubo_set;
  sets[1] = r->image_pool_sets[quad->texture];

  vkCmdBindVertexBuffers (r->active_frame_command_buffer, 0, 1, &vref.buffer,
                          &vref.offset);

  vkCmdBindIndexBuffer (r->active_frame_command_buffer, iref.buffer,
                        iref.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets (r->active_frame_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           r->active_pipeline_layout, 0, owl_array_size (sets),
                           sets, 1, &uref.offset32);

  vkCmdDrawIndexed (r->active_frame_command_buffer, owl_array_size (indices),
                    1, 0, 0, 0);

out:
  return code;
}

owl_private enum owl_code
owl_renderer_model_node_draw (struct owl_renderer *r, owl_m4 const matrix,
                              struct owl_model const *model,
                              owl_model_node_id nid)
{
  owl_i32 i;
  struct owl_model_node const *node;
  struct owl_model_mesh const *mesh;
  struct owl_model_skin const *skin;
  struct owl_model_skin_ssbo *ssbo;
  struct owl_model1_ubo ubo1;
  struct owl_model2_ubo ubo2;
  struct owl_renderer_frame_heap_reference u1ref;
  struct owl_renderer_frame_heap_reference u2ref;

  enum owl_code code = OWL_SUCCESS;

  node = &model->nodes[nid];

  for (i = 0; i < node->child_count; ++i) {
    code = owl_renderer_model_node_draw (r, matrix, model, node->children[i]);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

  if (OWL_MODEL_MESH_NONE == node->mesh) {
    goto out;
  }

  mesh = &model->meshes[node->mesh];
  skin = &model->skins[node->skin];
  ssbo = skin->ssbo_datas[r->active_frame];

  {
    owl_model_node_id pd;

    owl_m4_copy (node->matrix, ssbo->matrix);

    for (pd = node->parent; OWL_MODEL_NODE_PARENT_NONE != pd;
         pd = model->nodes[pd].parent) {
      owl_m4_multiply (model->nodes[pd].matrix, ssbo->matrix, ssbo->matrix);
    }
  }

  owl_m4_copy (r->camera_projection, ubo1.projection);
  owl_m4_copy (r->camera_view, ubo1.view);
  owl_m4_copy (matrix, ubo1.model);
  owl_v4_zero (ubo1.light);
  owl_v4_zero (ubo2.light_direction);

  code = owl_renderer_frame_heap_submit (r, sizeof (ubo1), &ubo1, &u1ref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  vkCmdBindDescriptorSets (r->active_frame_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           r->active_pipeline_layout, 0, 1,
                           &u1ref.model1_ubo_set, 1, &u1ref.offset32);

  code = owl_renderer_frame_heap_submit (r, sizeof (ubo1), &ubo1, &u2ref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  vkCmdBindDescriptorSets (r->active_frame_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           r->active_pipeline_layout, 5, 1,
                           &u2ref.model2_ubo_set, 1, &u2ref.offset32);

  for (i = 0; i < mesh->primitive_count; ++i) {
    VkDescriptorSet sets[4];
    struct owl_model_primitive const *primitive;
    struct owl_model_material const *material;
    struct owl_model_texture const *base_color_texture;
    struct owl_model_texture const *normal_texture;
    struct owl_model_texture const *physical_desc_texture;
    struct owl_model_image const *base_color_image;
    struct owl_model_image const *normal_image;
    struct owl_model_image const *physical_desc_image;
    struct owl_model_material_push_constant push_constant;

    primitive = &model->primitives[mesh->primitives[i]];

    if (!primitive->count) {
      continue;
    }

    material = &model->materials[primitive->material];

    base_color_texture = &model->textures[material->base_color_texture];
    normal_texture = &model->textures[material->normal_texture];
    physical_desc_texture = &model->textures[material->physical_desc_texture];

    base_color_image = &model->images[base_color_texture->model_image];
    normal_image = &model->images[normal_texture->model_image];
    physical_desc_image = &model->images[physical_desc_texture->model_image];

    sets[0] = r->image_pool_sets[base_color_image->renderer_image];
    sets[1] = r->image_pool_sets[normal_image->renderer_image];
    sets[2] = r->image_pool_sets[physical_desc_image->renderer_image];
    sets[3] = skin->ssbo_sets[r->active_frame];

    vkCmdBindDescriptorSets (
        r->active_frame_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        r->active_pipeline_layout, 1, owl_array_size (sets), sets, 0, NULL);

    push_constant.base_color_factor[0] = material->base_color_factor[0];
    push_constant.base_color_factor[1] = material->base_color_factor[1];
    push_constant.base_color_factor[2] = material->base_color_factor[2];
    push_constant.base_color_factor[3] = material->base_color_factor[3];

    push_constant.emissive_factor[0] = 0.0F;
    push_constant.emissive_factor[1] = 0.0F;
    push_constant.emissive_factor[2] = 0.0F;
    push_constant.emissive_factor[3] = 0.0F;

    push_constant.diffuse_factor[0] = 0.0F;
    push_constant.diffuse_factor[1] = 0.0F;
    push_constant.diffuse_factor[2] = 0.0F;
    push_constant.diffuse_factor[3] = 0.0F;

    push_constant.specular_factor[0] = 0.0F;
    push_constant.specular_factor[1] = 0.0F;
    push_constant.specular_factor[2] = 0.0F;
    push_constant.specular_factor[3] = 0.0F;

    push_constant.workflow = 0;

    /* FIXME(samuel): add uv sets in material */
    if (OWL_MODEL_TEXTURE_NONE == material->base_color_texture) {
      push_constant.base_color_uv_set = -1;
    } else {
      push_constant.base_color_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->physical_desc_texture) {
      push_constant.physical_desc_uv_set = -1;
    } else {
      push_constant.physical_desc_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->normal_texture) {
      push_constant.normal_uv_set = -1;
    } else {
      push_constant.normal_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->occlusion_texture) {
      push_constant.occlusion_uv_set = -1;
    } else {
      push_constant.occlusion_uv_set = 0;
    }

    if (OWL_MODEL_TEXTURE_NONE == material->emissive_texture) {
      push_constant.emissive_uv_set = -1;
    } else {
      push_constant.emissive_uv_set = 0;
    }

    push_constant.metallic_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.roughness_factor = 0.0F;
    push_constant.alpha_mask = 0.0F;
    push_constant.alpha_mask_cutoff = material->alpha_cutoff;

    vkCmdPushConstants (r->active_frame_command_buffer,
                        r->active_pipeline_layout,
                        VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof (push_constant), &push_constant);

    vkCmdDrawIndexed (r->active_frame_command_buffer, primitive->count, 1,
                      primitive->first, 0, 0);
  }

out:
  return code;
}

enum owl_code
owl_renderer_model_draw (struct owl_renderer *r, owl_m4 const matrix,
                         struct owl_model const *model)
{
  owl_i32 i;
  owl_u64 offset = 0;
  enum owl_code code = OWL_SUCCESS;

  vkCmdBindVertexBuffers (r->active_frame_command_buffer, 0, 1,
                          &model->vk_vertex_buffer, &offset);

  vkCmdBindIndexBuffer (r->active_frame_command_buffer, model->indices_buffer,
                        0, VK_INDEX_TYPE_UINT32);

  for (i = 0; i < model->root_count; ++i) {
    code = owl_renderer_model_node_draw (r, matrix, model, model->roots[i]);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

enum owl_code
owl_renderer_text_draw (struct owl_renderer *r, owl_v2 const pos,
                        owl_v3 const color, char const *text)
{
  char const *l;
  owl_v3 offset;
  owl_v3 scale;
  enum owl_code code = OWL_SUCCESS;

  offset[0] = pos[0] * (float)r->framebuffer_width;
  offset[1] = pos[1] * (float)r->framebuffer_height;
  offset[2] = 0.0F;

  scale[0] = 1.0F / (float)r->window_height;
  scale[1] = 1.0F / (float)r->window_height;
  scale[2] = 1.0F / (float)r->window_height;

  for (l = text; '\0' != *l; ++l) {
    owl_m4 matrix;
    struct owl_renderer_quad quad;
    struct owl_glyph glyph;

    owl_renderer_active_font_fill_glyph (r, *l, offset, &glyph);

    owl_m4_identity (matrix);
    owl_m4_scale_v3 (matrix, scale, matrix);

    quad.texture = r->font_pool_atlases[r->active_font];

    quad.position0[0] = glyph.positions[0][0];
    quad.position0[1] = glyph.positions[0][1];

    quad.position1[0] = glyph.positions[3][0];
    quad.position1[1] = glyph.positions[3][1];

    quad.color[0] = color[0];
    quad.color[1] = color[1];
    quad.color[2] = color[2];

    quad.uv0[0] = glyph.uvs[0][0];
    quad.uv0[1] = glyph.uvs[0][1];

    quad.uv1[0] = glyph.uvs[3][0];
    quad.uv1[1] = glyph.uvs[3][1];

    code = owl_renderer_quad_draw (r, matrix, &quad);
    if (OWL_SUCCESS != code) {
      goto out;
    }
  }

out:
  return code;
}

enum owl_code
owl_renderer_vertex_list_draw (struct owl_renderer *r, owl_m4 const matrix,
                               struct owl_renderer_vertex_list *list)
{
  owl_u64 sz;
  VkDescriptorSet sets[2];
  struct owl_renderer_common_ubo ubo;
  struct owl_renderer_frame_heap_reference vref;
  struct owl_renderer_frame_heap_reference iref;
  struct owl_renderer_frame_heap_reference uref;
  enum owl_code code = OWL_SUCCESS;

  sz = (owl_u64)list->vertex_count * sizeof (*list->vertices);
  code = owl_renderer_frame_heap_submit (r, sz, list->vertices, &vref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  sz = (owl_u64)list->index_count * sizeof (*list->indices);
  code = owl_renderer_frame_heap_submit (r, sz, list->indices, &iref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  owl_m4_copy (r->camera_projection, ubo.projection);
  owl_m4_copy (r->camera_view, ubo.view);
  owl_m4_copy (matrix, ubo.model);

  code = owl_renderer_frame_heap_submit (r, sizeof (ubo), &ubo, &uref);
  if (OWL_SUCCESS != code) {
    goto out;
  }

  sets[0] = uref.common_ubo_set;
  sets[1] = r->image_pool_sets[list->texture];

  vkCmdBindVertexBuffers (r->active_frame_command_buffer, 0, 1, &vref.buffer,
                          &vref.offset);

  vkCmdBindIndexBuffer (r->active_frame_command_buffer, iref.buffer,
                        iref.offset, VK_INDEX_TYPE_UINT32);

  vkCmdBindDescriptorSets (r->active_frame_command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           r->active_pipeline_layout, 0, owl_array_size (sets),
                           sets, 1, &uref.offset32);

  vkCmdDrawIndexed (r->active_frame_command_buffer, (owl_u32)list->index_count,
                    1, 0, 0, 0);

out:
  return code;
}

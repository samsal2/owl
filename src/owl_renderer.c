#include "owl_renderer.h"

#include "owl_definitions.h"
#include "owl_internal.h"
#include "owl_model.h"
#include "owl_plataform.h"
#include "owl_texture.h"
#include "owl_vector_math.h"

#ifndef OWL_POW
#include <math.h>
#define OWL_POW pow
#endif

#include <stdio.h>

#define OWL_DEFAULT_BUFFER_SIZE (1 << 16)

#if defined(OWL_ENABLE_VALIDATION)

static VKAPI_ATTR VKAPI_CALL VkBool32 owl_renderer_debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        VkDebugUtilsMessengerCallbackDataEXT const *data, void *user)
{
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
    "VK_LAYER_KHRONOS_validation"
};

#endif

static int owl_renderer_init_instance(struct owl_renderer *r)
{
    {
        int i;
        int ret;
        VkResult vk_result = VK_SUCCESS;

        uint32_t num_extensions;
        char const *const *extensions;

        VkApplicationInfo app_info;
        VkInstanceCreateInfo info;

        char const *name = owl_plataform_get_title(r->plataform);

        ret = owl_plataform_get_required_instance_extensions(
                r->plataform, &num_extensions, &extensions);
        if (ret)
            return ret;

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
#else /* OWL_ENABLE_VALIDATION */
        info.enabledLayerCount = 0;
        info.ppEnabledLayerNames = NULL;
#endif /* OWL_ENABLE_VALIDATION */
        info.enabledExtensionCount = num_extensions;
        info.ppEnabledExtensionNames = extensions;

        OWL_DEBUG_LOG("required extensions:\n");
        for (i = 0; i < (int)num_extensions; ++i)
            OWL_DEBUG_LOG(" %s\n", extensions[i]);

        vk_result = vkCreateInstance(&info, NULL, &r->instance);
        OWL_DEBUG_LOG("%i\n", vk_result);
        if (vk_result)
            goto error;
    }

#if defined(OWL_ENABLE_VALIDATION)
    r->vk_create_debug_utils_messenger_ext =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    r->instance, "vkCreateDebugUtilsMessengerEXT");
    if (!r->vk_create_debug_utils_messenger_ext)
        goto error_destroy_instance;

    r->vk_destroy_debug_utils_messenger_ext =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    r->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (!r->vk_destroy_debug_utils_messenger_ext)
        goto error_destroy_instance;

    r->vk_debug_marker_set_object_name_ext =
            (PFN_vkDebugMarkerSetObjectNameEXT)vkGetInstanceProcAddr(
                    r->instance, "vkDebugMarkerSetObjectNameEXT");
    if (!r->vk_destroy_debug_utils_messenger_ext)
        goto error_destroy_instance;

    {
        VkResult vk_result;
        VkDebugUtilsMessengerCreateInfoEXT info;

        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        info.pNext = NULL;
        info.flags = 0;
        info.messageSeverity = 0;
        info.messageSeverity |=
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        info.messageSeverity |=
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = 0;
        info.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
        info.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        info.messageType |= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = owl_renderer_debug_callback;
        info.pUserData = r;

        vk_result = r->vk_create_debug_utils_messenger_ext(
                r->instance, &info, NULL, &r->debug_messenger);
        if (vk_result)
            goto error_destroy_instance;
    }
#else
    r->debug_messenger = NULL;
    r->vk_create_debug_utils_messenger_ext = NULL;
    r->vk_destroy_debug_utils_messenger_ext = NULL;
    r->vk_debug_marker_set_object_name_ext = NULL;
#endif

    return OWL_OK;

#if defined(OWL_ENABLE_VALIDATION)
error_destroy_instance:
    vkDestroyInstance(r->instance, NULL);
#endif

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_instance(struct owl_renderer *r)
{
    VkInstance const instance = r->instance;
#if defined(OWL_ENABLE_VALIDATION)
    r->vk_destroy_debug_utils_messenger_ext(instance, r->debug_messenger,
                                            NULL);
#endif
    vkDestroyInstance(instance, NULL);
}

#if defined(OWL_ENABLE_VALIDATION) && 0
#define OWL_RENDERER_SET_VK_OBJECT_NAME(r, handle, type, name) \
    owl_renderer_set_vk_object_name(r, handle, type, name)
static void owl_renderer_set_vk_object_name(struct owl_renderer *r,
                                            uint64_t handle,
                                            VkDebugReportObjectTypeEXT type,
                                            char const *name)
{
    VkDebugMarkerObjectNameInfoEXT info;
    VkDevice const device = r->device;

    info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
    info.pNext = NULL;
    info.objectType = type;
    info.object = handle;
    info.pObjectName = name;

    r->vk_debug_marker_set_object_name_ext(device, &info);
}
#else
#define OWL_RENDERER_SET_VK_OBJECT_NAME(r, handle, type, name)
#endif

static int owl_renderer_init_surface(struct owl_renderer *r)
{
    return owl_plataform_create_vulkan_surface(r->plataform, r);
}

static void owl_renderer_deinit_surface(struct owl_renderer *r)
{
    vkDestroySurfaceKHR(r->instance, r->surface, NULL);
}

static char const *const device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/* TODO(samuel): refactor this function, it's way too big */
static int owl_renderer_select_physical_device(struct owl_renderer *r)
{
    uint32_t i;

    int32_t device_found = 0;

    int ret = OWL_OK;
    VkResult vk_result = VK_SUCCESS;

    uint32_t num_devices = 0;
    VkPhysicalDevice *devices = NULL;

    uint32_t num_queue_family_properties = 0;
    VkQueueFamilyProperties *queue_family_properties = NULL;

    int32_t *extensions_found = NULL;
    uint32_t num_extension_properties = 0;
    VkExtensionProperties *extension_properties = NULL;

    uint32_t num_formats = 0;
    VkSurfaceFormatKHR *formats = NULL;

    extensions_found = OWL_MALLOC(OWL_ARRAY_SIZE(device_extensions) *
                                  sizeof(*extensions_found));
    if (!extensions_found) {
        ret = OWL_ERROR_NO_MEMORY;
        goto cleanup;
    }

    vk_result = vkEnumeratePhysicalDevices(r->instance, &num_devices, NULL);
    if (vk_result) {
        ret = OWL_ERROR_FATAL;
        goto cleanup;
    }

    devices = OWL_MALLOC(num_devices * sizeof(*devices));
    if (!devices) {
        ret = OWL_ERROR_NO_MEMORY;
        goto cleanup;
    }

    vk_result = vkEnumeratePhysicalDevices(r->instance, &num_devices, devices);
    if (vk_result) {
        ret = OWL_ERROR_FATAL;
        goto cleanup;
    }

    for (i = 0; i < num_devices && !device_found; ++i) {
        uint32_t j;
        VkPhysicalDeviceProperties device_properties;
        VkSampleCountFlagBits requested_samples = VK_SAMPLE_COUNT_2_BIT;
        VkSampleCountFlagBits supported_samples = 0;
        VkFormat requested_format = VK_FORMAT_B8G8R8A8_SRGB;
        VkColorSpaceKHR requested_color_space =
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        uint32_t num_present_modes;
        VkPresentModeKHR present_modes[16];
        int32_t found_present_mode = 0;
#if 1
        VkPresentModeKHR requested_present_mode =
                VK_PRESENT_MODE_IMMEDIATE_KHR;
#else
        VkPresentModeKHR requested_present_mode = VK_PRESENT_MODE_FIFO_KHR;
#endif
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
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, r->surface, &has_formats,
                                             NULL);
        if (!has_formats)
            continue;

        OWL_DEBUG_LOG("  device %p contains surface formats!\n", device);

        /* has present modes? if not go next device */
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, r->surface,
                                                  &has_present_modes, NULL);
        if (!has_present_modes)
            continue;

        OWL_DEBUG_LOG("  device %p contains present modes!\n", device);

        /* allocate the queue_family_properties, no memory? cleanup and return */
        vkGetPhysicalDeviceQueueFamilyProperties(
                device, &num_queue_family_properties, NULL);

        check_realloc = OWL_REALLOC(queue_family_properties,
                                    num_queue_family_properties *
                                            sizeof(*queue_family_properties));
        if (!check_realloc) {
            ret = OWL_ERROR_NO_MEMORY;
            goto cleanup;
        }

        queue_family_properties = check_realloc;
        vkGetPhysicalDeviceQueueFamilyProperties(
                device, &num_queue_family_properties, queue_family_properties);

        /* for each family, if said family has the required properties set the
       corresponding family to it's index */
        for (current_family = 0;
             current_family < num_queue_family_properties &&
             ((uint32_t)-1 == graphics_family ||
              (uint32_t)-1 == present_family ||
              (uint32_t)-1 == compute_family);
             ++current_family) {
            VkBool32 has_surface;
            VkQueueFamilyProperties *properties;

            properties = &queue_family_properties[current_family];

            vkGetPhysicalDeviceSurfaceSupportKHR(device, current_family,
                                                 r->surface, &has_surface);

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
        if ((uint32_t)-1 == graphics_family ||
            (uint32_t)-1 == present_family || (uint32_t)-1 == compute_family)
            continue;

        OWL_DEBUG_LOG("  device %p has the required families!\n", device);

        /* vulkan error, cleanup and return */
        vk_result = vkEnumerateDeviceExtensionProperties(
                device, NULL, &num_extension_properties, NULL);
        if (vk_result) {
            ret = OWL_ERROR_FATAL;
            goto cleanup;
        }

        check_realloc = OWL_REALLOC(extension_properties,
                                    num_extension_properties *
                                            sizeof(*extension_properties));
        if (!check_realloc) {
            ret = OWL_ERROR_NO_MEMORY;
            goto cleanup;
        }

        extension_properties = check_realloc;
        vk_result = vkEnumerateDeviceExtensionProperties(
                device, NULL, &num_extension_properties, extension_properties);
        if (vk_result) {
            ret = OWL_ERROR_FATAL;
            goto cleanup;
        }

        for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j)
            extensions_found[j] = 0;

        for (j = 0; j < OWL_ARRAY_SIZE(device_extensions); ++j) {
            uint32_t k;
            char const *required = device_extensions[j];
            uint32_t const required_length = OWL_STRLEN(required);

            OWL_DEBUG_LOG("  device %p looking at %s\n", device, required);

            for (k = 0; k < num_extension_properties && !extensions_found[j];
                 ++k) {
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
        for (j = 0; j < OWL_ARRAY_SIZE(device_extensions) && has_extensions;
             ++j)
            if (!extensions_found[j])
                has_extensions = 0;

        /* if it's missing device extensions, go next */
        if (!has_extensions)
            continue;

        OWL_DEBUG_LOG("  device %p has the required extensions!\n", device);

        /* congratulations! a device has been found! */
        device_found = 1;
        r->physical_device = device;
        r->graphics_family = graphics_family;
        r->present_family = present_family;
        r->compute_family = compute_family;

        vkGetPhysicalDeviceProperties(device, &device_properties);

        supported_samples = 0;
        supported_samples |=
                device_properties.limits.framebufferColorSampleCounts;
        supported_samples |=
                device_properties.limits.framebufferDepthSampleCounts;

        if (VK_SAMPLE_COUNT_1_BIT & requested_samples) {
            OWL_DEBUG_LOG(
                    "VK_SAMPLE_COUNT_1_BIT not supported, falling back to "
                    "VK_SAMPLE_COUNT_2_BIT\n");
            r->msaa = VK_SAMPLE_COUNT_2_BIT;
        } else if (supported_samples & requested_samples) {
            r->msaa = requested_samples;
        } else {
            OWL_DEBUG_LOG("falling back to VK_SAMPLE_COUNT_2_BIT\n");
            r->msaa = VK_SAMPLE_COUNT_2_BIT;
        }

        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, r->surface,
                                                         &num_formats, NULL);

        check_realloc = OWL_REALLOC(formats, num_formats * sizeof(*formats));
        if (!check_realloc) {
            ret = OWL_ERROR_NO_MEMORY;
            goto cleanup;
        }

        formats = check_realloc;
        vk_result = vkGetPhysicalDeviceSurfaceFormatsKHR(
                device, r->surface, &num_formats, formats);

        for (j = 0; j < num_formats && !found_format; ++j)
            if (formats[j].format == requested_format &&
                formats[j].colorSpace == requested_color_space)
                found_format = 1;

        /* no suitable format, go next */
        if (!found_format) {
            ret = OWL_ERROR_FATAL;
            goto cleanup;
        }

        r->surface_format.format = requested_format;
        r->surface_format.colorSpace = requested_color_space;

        vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, r->surface, &num_present_modes, NULL);

        if (vk_result || OWL_ARRAY_SIZE(present_modes) < num_present_modes) {
            ret = OWL_ERROR_FATAL;
            goto cleanup;
        }

        vk_result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, r->surface, &num_present_modes, present_modes);

        for (j = 0; j < num_present_modes; ++j)
            if (requested_present_mode == present_modes[j])
                found_present_mode = 1;

        /* no suitable present mode, go next */
        if (!found_present_mode) {
            OWL_DEBUG_LOG("falling back to VK_PRESENT_MODE_FIFO_KHR\n");
            r->present_mode = VK_PRESENT_MODE_FIFO_KHR;
        } else {
            OWL_DEBUG_LOG("found the required present mode!\n");
            r->present_mode = requested_present_mode;
        }

        vkGetPhysicalDeviceFormatProperties(device,
                                            VK_FORMAT_D24_UNORM_S8_UINT,
                                            &d24_unorm_s8_uint_properties);

        vkGetPhysicalDeviceFormatProperties(device,
                                            VK_FORMAT_D32_SFLOAT_S8_UINT,
                                            &d32_sfloat_s8_uint_properties);

        if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
            d24_unorm_s8_uint_properties.optimalTilingFeatures) {
            r->depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
        } else if (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT &
                   d32_sfloat_s8_uint_properties.optimalTilingFeatures) {
            r->depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        } else {
            OWL_DEBUG_LOG("could't find depth format\n");
            ret = OWL_ERROR_FATAL;
            goto cleanup;
        }
    }

    if (!device_found)
        ret = OWL_ERROR_FATAL;

cleanup:
    OWL_FREE(devices);
    OWL_FREE(queue_family_properties);
    OWL_FREE(extensions_found);
    OWL_FREE(extension_properties);
    OWL_FREE(formats);

    return ret;
}

static int owl_renderer_init_device(struct owl_renderer *r)
{
    VkPhysicalDeviceFeatures features;
    VkDeviceCreateInfo info;
    VkDeviceQueueCreateInfo queue_infos[2];
    float const priority = 1.0F;
    VkResult vk_result = VK_SUCCESS;
    int ret = OWL_OK;

    queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[0].pNext = NULL;
    queue_infos[0].flags = 0;
    queue_infos[0].queueFamilyIndex = r->graphics_family;
    queue_infos[0].queueCount = 1;
    queue_infos[0].pQueuePriorities = &priority;

    queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[1].pNext = NULL;
    queue_infos[1].flags = 0;
    queue_infos[1].queueFamilyIndex = r->present_family;
    queue_infos[1].queueCount = 1;
    queue_infos[1].pQueuePriorities = &priority;

    vkGetPhysicalDeviceFeatures(r->physical_device, &features);

    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    if (r->graphics_family == r->present_family) {
        info.queueCreateInfoCount = 1;
    } else {
        info.queueCreateInfoCount = 2;
    }
    info.pQueueCreateInfos = queue_infos;
    info.enabledLayerCount = 0; /* deprecated */
    info.ppEnabledLayerNames = NULL; /* deprecated */
    info.enabledExtensionCount = OWL_ARRAY_SIZE(device_extensions);
    info.ppEnabledExtensionNames = device_extensions;
    info.pEnabledFeatures = &features;

    vk_result = vkCreateDevice(r->physical_device, &info, NULL, &r->device);
    if (vk_result)
        return OWL_ERROR_FATAL;

    vkGetDeviceQueue(r->device, r->graphics_family, 0, &r->graphics_queue);

    vkGetDeviceQueue(r->device, r->present_family, 0, &r->present_queue);

    vkGetDeviceQueue(r->device, r->present_family, 0, &r->compute_queue);

    return ret;
}

static void owl_renderer_deinit_device(struct owl_renderer *r)
{
    vkDestroyDevice(r->device, NULL);
}

static int owl_renderer_clamp_dimensions(struct owl_renderer *r)
{
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult vk_result;

    vk_result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            r->physical_device, r->surface, &capabilities);
    if (vk_result)
        return OWL_ERROR_FATAL;

    if ((uint32_t)-1 != capabilities.currentExtent.width) {
        uint32_t const min_width = capabilities.minImageExtent.width;
        uint32_t const max_width = capabilities.maxImageExtent.width;
        uint32_t const min_height = capabilities.minImageExtent.height;
        uint32_t const max_height = capabilities.maxImageExtent.height;

        r->width = OWL_CLAMP(r->width, min_width, max_width);
        r->height = OWL_CLAMP(r->height, min_height, max_height);
    }

    return OWL_OK;
}

static int owl_renderer_init_attachments(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    {
        VkImageCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = r->surface_format.format;
        info.extent.width = r->width;
        info.extent.height = r->height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = r->msaa;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = NULL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vk_result = vkCreateImage(device, &info, NULL, &r->color_image);
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

        vkGetImageMemoryRequirements(device, r->color_image, &requirements);

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = requirements.size;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL, &r->color_memory);
        if (vk_result)
            goto error_destroy_color_image;

        vk_result =
                vkBindImageMemory(device, r->color_image, r->color_memory, 0);
        if (vk_result)
            goto error_free_color_memory;
    }

    {
        VkImageViewCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.image = r->color_image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = r->surface_format.format;
        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        vk_result =
                vkCreateImageView(device, &info, NULL, &r->color_image_view);
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
        info.format = r->depth_format;
        info.extent.width = r->width;
        info.extent.height = r->height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = r->msaa;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = NULL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vk_result = vkCreateImage(r->device, &info, NULL, &r->depth_image);
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

        vkGetImageMemoryRequirements(device, r->depth_image, &requirements);

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = requirements.size;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL, &r->depth_memory);
        if (vk_result)
            goto error_destroy_depth_image;

        vk_result =
                vkBindImageMemory(device, r->depth_image, r->depth_memory, 0);
        if (vk_result)
            goto error_free_depth_memory;
    }
    {
        VkImageViewCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.image = r->depth_image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = r->depth_format;
        info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        vk_result =
                vkCreateImageView(device, &info, NULL, &r->depth_image_view);
        if (vk_result)
            goto error_free_depth_memory;
    }

    return OWL_OK;

error_free_depth_memory:
    vkFreeMemory(device, r->depth_memory, NULL);

error_destroy_depth_image:
    vkDestroyImage(device, r->depth_image, NULL);

error_destroy_color_image_view:
    vkDestroyImageView(device, r->color_image_view, NULL);

error_free_color_memory:
    vkFreeMemory(device, r->color_memory, NULL);

error_destroy_color_image:
    vkDestroyImage(device, r->color_image, NULL);

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_attachments(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    vkDestroyImageView(device, r->depth_image_view, NULL);
    vkFreeMemory(device, r->depth_memory, NULL);
    vkDestroyImage(device, r->depth_image, NULL);
    vkDestroyImageView(device, r->color_image_view, NULL);
    vkFreeMemory(device, r->color_memory, NULL);
    vkDestroyImage(device, r->color_image, NULL);
}

static int owl_renderer_init_render_passes(struct owl_renderer *r)
{
    VkAttachmentReference color_reference;
    VkAttachmentReference depth_reference;
    VkAttachmentReference resolve_reference;
    VkAttachmentDescription attachments[3];
    VkSubpassDescription subpass;
    VkSubpassDependency dependency;
    VkRenderPassCreateInfo info;
    VkResult vk_result;
    VkDevice const device = r->device;

    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    resolve_reference.attachment = 2;
    resolve_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* color */
    attachments[0].flags = 0;
    attachments[0].format = r->surface_format.format;
    attachments[0].samples = r->msaa;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* depth */
    attachments[1].flags = 0;
    attachments[1].format = r->depth_format;
    attachments[1].samples = r->msaa;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

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
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = &resolve_reference;
    subpass.pDepthStencilAttachment = &depth_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = 0;
    dependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = 0;
    dependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = 0;
    dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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

    vk_result = vkCreateRenderPass(device, &info, NULL, &r->main_render_pass);
    if (vk_result)
        goto error;

    return OWL_OK;

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_render_passes(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyRenderPass(device, r->main_render_pass, NULL);
}

static int owl_renderer_init_swapchain(struct owl_renderer *r)
{
    int32_t i;

    VkResult vk_result = VK_SUCCESS;
    int ret = OWL_OK;
    VkDevice const device = r->device;
    VkPhysicalDevice const physical_device = r->physical_device;

    {
        uint32_t families[2];
        VkSwapchainCreateInfoKHR info;
        VkSurfaceCapabilitiesKHR capabilities;

        r->swapchain_image = 0;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, r->surface,
                                                  &capabilities);

        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.pNext = NULL;
        info.flags = 0;
        info.surface = r->surface;
        info.minImageCount = r->num_frames;
        info.imageFormat = r->surface_format.format;
        info.imageColorSpace = r->surface_format.colorSpace;
        info.imageExtent.width = r->width;
        info.imageExtent.height = r->height;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.preTransform = capabilities.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = r->present_mode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;

        families[0] = r->graphics_family;
        families[1] = r->present_family;

        if (r->graphics_family == r->present_family) {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = NULL;
        } else {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families;
        }

        vk_result = vkCreateSwapchainKHR(device, &info, NULL, &r->swapchain);
        if (vk_result)
            goto error;

        vk_result = vkGetSwapchainImagesKHR(device, r->swapchain,
                                            &r->num_swapchain_images, NULL);
        if (vk_result || OWL_MAX_SWAPCHAIN_IMAGES <= r->num_swapchain_images)
            goto error_destroy_swapchain;

        vk_result = vkGetSwapchainImagesKHR(device, r->swapchain,
                                            &r->num_swapchain_images,
                                            r->swapchain_images);
        if (vk_result)
            goto error_destroy_swapchain;
    }

    for (i = 0; i < (int32_t)r->num_swapchain_images; ++i) {
        VkImageViewCreateInfo info;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.image = r->swapchain_images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = r->surface_format.format;
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
                                      &r->swapchain_image_views[i]);
        if (vk_result)
            goto error_destroy_image_views;
    }

    for (i = 0; i < (int32_t)r->num_swapchain_images; ++i) {
        VkImageView attachments[3];
        VkFramebufferCreateInfo info;

        attachments[0] = r->color_image_view;
        attachments[1] = r->depth_image_view;
        attachments[2] = r->swapchain_image_views[i];

        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.renderPass = r->main_render_pass;
        info.attachmentCount = OWL_ARRAY_SIZE(attachments);
        info.pAttachments = attachments;
        info.width = r->width;
        info.height = r->height;
        info.layers = 1;

        vk_result = vkCreateFramebuffer(device, &info, NULL,
                                        &r->swapchain_framebuffers[i]);
        if (vk_result)
            goto error_destroy_framebuffers;
    }

    return OWL_OK;

error_destroy_framebuffers:
    for (i = i - 1; i >= 0; --i)
        vkDestroyFramebuffer(device, r->swapchain_framebuffers[i], NULL);

    i = r->num_swapchain_images;

error_destroy_image_views:
    for (i = i - 1; i >= 0; --i)
        vkDestroyImageView(device, r->swapchain_image_views[i], NULL);

error_destroy_swapchain:
    vkDestroySwapchainKHR(device, r->swapchain, NULL);

error:
    return ret;
}

static void owl_renderer_deinit_swapchain(struct owl_renderer *r)
{
    uint32_t i;
    VkDevice const device = r->device;

    for (i = 0; i < r->num_swapchain_images; ++i)
        vkDestroyFramebuffer(device, r->swapchain_framebuffers[i], NULL);

    for (i = 0; i < r->num_swapchain_images; ++i)
        vkDestroyImageView(device, r->swapchain_image_views[i], NULL);

    vkDestroySwapchainKHR(device, r->swapchain, NULL);
}

static int owl_renderer_init_pools(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    {
        VkCommandPoolCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.pNext = NULL;
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.queueFamilyIndex = r->graphics_family;

        vk_result = vkCreateCommandPool(device, &info, NULL, &r->command_pool);
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
                                           &r->descriptor_pool);
        if (vk_result)
            goto error_destroy_command_pool;
    }

    return OWL_OK;

error_destroy_command_pool:
    vkDestroyCommandPool(device, r->command_pool, NULL);

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_pools(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyDescriptorPool(device, r->descriptor_pool, NULL);
    vkDestroyCommandPool(device, r->command_pool, NULL);
}

static int owl_renderer_init_shaders(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    {
        VkShaderModuleCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        /* TODO(samuel): Properly load ret at runtime */
        static uint32_t const spv[] = {
#include "owl_basic.vert.spv.u32"
        };

        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.codeSize = sizeof(spv);
        info.pCode = spv;

        vk_result = vkCreateShaderModule(device, &info, NULL,
                                         &r->basic_vertex_shader);
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
                                         &r->basic_fragment_shader);
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
                                         &r->text_fragment_shader);

        if (vk_result)
            goto error_destroy_basic_fragment_shader;
    }

    {
        VkShaderModuleCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        static uint32_t const spv[] = {
#include "owl_pbr.vert.spv.u32"
        };

        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.codeSize = sizeof(spv);
        info.pCode = spv;

        vk_result = vkCreateShaderModule(device, &info, NULL,
                                         &r->model_vertex_shader);

        if (VK_SUCCESS != vk_result)
            goto error_destroy_text_fragment_shader;
    }

    {
        VkShaderModuleCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        static uint32_t const spv[] = {
#include "owl_pbr.frag.spv.u32"
        };

        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.codeSize = sizeof(spv);
        info.pCode = spv;

        vk_result = vkCreateShaderModule(device, &info, NULL,
                                         &r->model_fragment_shader);

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
                                         &r->skybox_vertex_shader);

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
                                         &r->skybox_fragment_shader);

        if (vk_result)
            goto error_destroy_skybox_vertex_shader;
    }

    return OWL_OK;

error_destroy_skybox_vertex_shader:
    vkDestroyShaderModule(device, r->skybox_vertex_shader, NULL);

error_destroy_model_fragment_shader:
    vkDestroyShaderModule(device, r->model_fragment_shader, NULL);

error_destroy_model_vertex_shader:
    vkDestroyShaderModule(device, r->model_vertex_shader, NULL);

error_destroy_text_fragment_shader:
    vkDestroyShaderModule(device, r->text_fragment_shader, NULL);

error_destroy_basic_fragment_shader:
    vkDestroyShaderModule(device, r->basic_fragment_shader, NULL);

error_destroy_basic_vertex_shader:
    vkDestroyShaderModule(device, r->basic_vertex_shader, NULL);

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_shaders(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyShaderModule(device, r->skybox_fragment_shader, NULL);
    vkDestroyShaderModule(device, r->skybox_vertex_shader, NULL);
    vkDestroyShaderModule(device, r->model_fragment_shader, NULL);
    vkDestroyShaderModule(device, r->model_vertex_shader, NULL);
    vkDestroyShaderModule(device, r->text_fragment_shader, NULL);
    vkDestroyShaderModule(device, r->basic_fragment_shader, NULL);
    vkDestroyShaderModule(device, r->basic_vertex_shader, NULL);
}

static int owl_renderer_init_layouts(struct owl_renderer *r)
{
    VkDevice const device = r->device;

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
                device, &info, NULL, &r->common_uniform_descriptor_set_layout);
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
                device, &info, NULL, &r->common_texture_descriptor_set_layout);
        if (vk_result)
            goto error_destroy_common_uniform_descriptor_set_layout;
    }

    {
        VkDescriptorSetLayout layouts[2];
        VkPipelineLayoutCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        layouts[0] = r->common_uniform_descriptor_set_layout;
        layouts[1] = r->common_texture_descriptor_set_layout;

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
        info.pSetLayouts = layouts;
        info.pushConstantRangeCount = 0;
        info.pPushConstantRanges = NULL;

        vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                           &r->common_pipeline_layout);
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
        binding.stageFlags = 0;
        binding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        binding.pImmutableSamplers = NULL;

        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.bindingCount = 1;
        info.pBindings = &binding;

        vk_result = vkCreateDescriptorSetLayout(
                device, &info, NULL, &r->model_uniform_descriptor_set_layout);
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
                device, &info, NULL, &r->model_storage_descriptor_set_layout);
        if (vk_result)
            goto error_destroy_model_uniform_descriptor_set_layout;
    }

    {
        VkDescriptorSetLayoutBinding bindings[6];
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

        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[4].pImmutableSamplers = NULL;

        bindings[5].binding = 5;
        bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[5].descriptorCount = 1;
        bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[5].pImmutableSamplers = NULL;

        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.bindingCount = OWL_ARRAY_SIZE(bindings);
        info.pBindings = bindings;

        vk_result = vkCreateDescriptorSetLayout(
                device, &info, NULL, &r->model_maps_descriptor_set_layout);
        if (vk_result)
            goto error_destroy_model_storage_descriptor_set_layout;
    }

    {
        VkDescriptorSetLayoutBinding bindings[3];
        VkDescriptorSetLayoutCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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

        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.bindingCount = OWL_ARRAY_SIZE(bindings);
        info.pBindings = bindings;

        vk_result = vkCreateDescriptorSetLayout(
                device, &info, NULL,
                &r->model_environment_descriptor_set_layout);
        if (vk_result)
            goto error_destroy_model_maps_descriptor_set_layout;
    }

    {
        VkDescriptorSetLayout layouts[4];
        VkPushConstantRange push_constant;
        VkPipelineLayoutCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constant.offset = 0;
        push_constant.size = sizeof(struct owl_model_push_constant);

        layouts[0] = r->model_uniform_descriptor_set_layout;
        layouts[1] = r->model_storage_descriptor_set_layout;
        layouts[2] = r->model_maps_descriptor_set_layout;
        layouts[3] = r->model_environment_descriptor_set_layout;

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.setLayoutCount = OWL_ARRAY_SIZE(layouts);
        info.pSetLayouts = layouts;
        info.pushConstantRangeCount = 1;
        info.pPushConstantRanges = &push_constant;

        vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                           &r->model_pipeline_layout);
        if (vk_result)
            goto error_destroy_model_environment_descriptor_set_layout;
    }

    return OWL_OK;

error_destroy_model_environment_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(
            device, r->model_environment_descriptor_set_layout, NULL);

error_destroy_model_maps_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(device, r->model_maps_descriptor_set_layout,
                                 NULL);

error_destroy_model_storage_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(device,
                                 r->model_storage_descriptor_set_layout, NULL);

error_destroy_model_uniform_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(device,
                                 r->model_uniform_descriptor_set_layout, NULL);

error_destroy_common_pipeline_layout:
    vkDestroyPipelineLayout(device, r->common_pipeline_layout, NULL);

error_destroy_common_texture_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(
            device, r->common_texture_descriptor_set_layout, NULL);

error_destroy_common_uniform_descriptor_set_layout:
    vkDestroyDescriptorSetLayout(
            device, r->common_uniform_descriptor_set_layout, NULL);

error:
    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_layouts(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyPipelineLayout(device, r->model_pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(
            device, r->model_environment_descriptor_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device, r->model_maps_descriptor_set_layout,
                                 NULL);
    vkDestroyDescriptorSetLayout(device,
                                 r->model_storage_descriptor_set_layout, NULL);
    vkDestroyDescriptorSetLayout(device,
                                 r->model_uniform_descriptor_set_layout, NULL);
    vkDestroyPipelineLayout(device, r->common_pipeline_layout, NULL);
    vkDestroyDescriptorSetLayout(
            device, r->common_texture_descriptor_set_layout, NULL);
    vkDestroyDescriptorSetLayout(
            device, r->common_uniform_descriptor_set_layout, NULL);
}

static int owl_renderer_init_graphics_pipelines(struct owl_renderer *r)
{
    VkVertexInputBindingDescription vertex_bindings;
    VkVertexInputAttributeDescription vertex_attributes[7];
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
    int ret;
    VkResult vk_result;
    VkDevice const device = r->device;

    ret = owl_renderer_init_shaders(r);
    if (ret)
        return ret;

    vertex_bindings.binding = 0;
    vertex_bindings.stride = sizeof(struct owl_common_vertex);
    vertex_bindings.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_common_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_common_vertex, color);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_common_vertex, uv);

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
    viewport.width = r->width;
    viewport.height = r->height;
    viewport.minDepth = 0.0F;
    viewport.maxDepth = 1.0F;

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = r->width;
    scissor.extent.height = r->height;

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
    multisample.rasterizationSamples = r->msaa;
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
    color_attachment.colorWriteMask = 0;
    color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

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
    info.layout = r->common_pipeline_layout;
    info.renderPass = r->main_render_pass;
    info.subpass = 0;
    info.basePipelineHandle = VK_NULL_HANDLE;
    info.basePipelineIndex = -1;

    vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &r->basic_pipeline);
    if (vk_result)
        goto error_denit_shaders;

    rasterization.polygonMode = VK_POLYGON_MODE_LINE;

    vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &r->wires_pipeline);
    if (vk_result)
        goto error_destroy_basic_pipeline;

    rasterization.polygonMode = VK_POLYGON_MODE_FILL;

    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    stages[1].module = r->text_fragment_shader;

    vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &r->text_pipeline);
    if (vk_result)
        goto error_destroy_wires_pipeline;

    vertex_bindings.stride = sizeof(struct owl_model_vertex);

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_model_vertex, position);

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[1].offset = offsetof(struct owl_model_vertex, normal);

    vertex_attributes[2].binding = 0;
    vertex_attributes[2].location = 2;
    vertex_attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[2].offset = offsetof(struct owl_model_vertex, uv0);

    vertex_attributes[3].binding = 0;
    vertex_attributes[3].location = 3;
    vertex_attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes[3].offset = offsetof(struct owl_model_vertex, uv1);

    vertex_attributes[4].binding = 0;
    vertex_attributes[4].location = 4;
    vertex_attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attributes[4].offset = offsetof(struct owl_model_vertex, joints0);

    vertex_attributes[5].binding = 0;
    vertex_attributes[5].location = 5;
    vertex_attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attributes[5].offset = offsetof(struct owl_model_vertex, weights0);

    vertex_attributes[6].binding = 0;
    vertex_attributes[6].location = 6;
    vertex_attributes[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertex_attributes[6].offset = offsetof(struct owl_model_vertex, color0);

    vertex_input.vertexAttributeDescriptionCount = 7;

    color_attachment.blendEnable = VK_FALSE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

    stages[0].module = r->model_vertex_shader;
    stages[1].module = r->model_fragment_shader;

    info.layout = r->model_pipeline_layout;

    vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &r->model_pipeline);
    if (vk_result)
        goto error_destroy_text_pipeline;

    vertex_bindings.stride = sizeof(struct owl_skybox_vertex);

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes[0].offset = offsetof(struct owl_skybox_vertex, position);

    vertex_input.vertexAttributeDescriptionCount = 1;

    depth.depthTestEnable = VK_FALSE;
    depth.depthWriteEnable = VK_FALSE;

    stages[0].module = r->skybox_vertex_shader;
    stages[1].module = r->skybox_fragment_shader;

    info.layout = r->common_pipeline_layout;

    vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                          NULL, &r->skybox_pipeline);

    if (vk_result)
        goto error_destroy_model_pipeline;

    owl_renderer_deinit_shaders(r);

    return OWL_OK;

error_destroy_model_pipeline:
    vkDestroyPipeline(device, r->model_pipeline, NULL);

error_destroy_text_pipeline:
    vkDestroyPipeline(device, r->text_pipeline, NULL);

error_destroy_wires_pipeline:
    vkDestroyPipeline(device, r->wires_pipeline, NULL);

error_destroy_basic_pipeline:
    vkDestroyPipeline(device, r->basic_pipeline, NULL);

error_denit_shaders:
    owl_renderer_deinit_shaders(r);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_graphics_pipelines(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyPipeline(device, r->skybox_pipeline, NULL);
    vkDestroyPipeline(device, r->model_pipeline, NULL);
    vkDestroyPipeline(device, r->text_pipeline, NULL);
    vkDestroyPipeline(device, r->wires_pipeline, NULL);
    vkDestroyPipeline(device, r->basic_pipeline, NULL);
}

static int owl_renderer_init_upload_buffer(struct owl_renderer *r)
{
    r->upload_buffer_in_use = 0;
    return OWL_OK;
}

static void owl_renderer_deinit_upload_buffer(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    if (r->upload_buffer_in_use) { /* someone forgot to call free */
        vkFreeMemory(device, r->upload_buffer_memory, NULL);
        vkDestroyBuffer(device, r->upload_buffer, NULL);
    }
}

static int owl_renderer_init_garbage(struct owl_renderer *r)
{
    uint32_t i;

    r->garbage = 0;

    for (i = 0; i < OWL_NUM_GARBAGE_FRAMES; ++i) {
        r->num_garbage_buffers[i] = 0;
        r->num_garbage_memories[i] = 0;
        r->num_garbage_descriptor_sets[i] = 0;
    }

    return OWL_OK;
}

static void owl_renderer_collect_garbage(struct owl_renderer *r)
{
    uint32_t i;
    uint32_t const garbage_frames = OWL_NUM_GARBAGE_FRAMES;
    /* use the _oldest_ garbage framt to collect */
    uint32_t const collect = (r->garbage + 2) % garbage_frames;
    VkDevice const device = r->device;

    /* update the garbage index */
    r->garbage = (r->garbage + 1) % garbage_frames;

    if (r->num_garbage_descriptor_sets[collect]) {
        vkFreeDescriptorSets(device, r->descriptor_pool,
                             r->num_garbage_descriptor_sets[collect],
                             r->garbage_descriptor_sets[collect]);
    }

    r->num_garbage_descriptor_sets[collect] = 0;

    for (i = 0; i < r->num_garbage_memories[collect]; ++i) {
        VkDeviceMemory memory = r->garbage_memories[collect][i];
        vkFreeMemory(device, memory, NULL);
    }

    r->num_garbage_memories[collect] = 0;

    for (i = 0; i < r->num_garbage_buffers[collect]; ++i) {
        VkBuffer buffer = r->garbage_buffers[collect][i];
        vkDestroyBuffer(device, buffer, NULL);
    }

    r->num_garbage_buffers[collect] = 0;
}

static void owl_renderer_deinit_garbage(struct owl_renderer *r)
{
    r->garbage = 0;
    for (; r->garbage < OWL_NUM_GARBAGE_FRAMES; ++r->garbage)
        owl_renderer_collect_garbage(r);
}

static int owl_renderer_garbage_push_vertex(struct owl_renderer *r)
{
    uint32_t i;
    uint32_t const capacity = OWL_ARRAY_SIZE(r->garbage_buffers[0]);
    uint32_t const garbage = r->garbage;
    uint32_t const num_buffers = r->num_garbage_buffers[garbage];
    uint32_t const num_memories = r->num_garbage_memories[garbage];

    if (capacity <= num_buffers + r->num_frames)
        return OWL_ERROR_NO_SPACE;

    if (capacity <= num_memories + 1)
        return OWL_ERROR_NO_SPACE;

    for (i = 0; i < r->num_frames; ++i) {
        VkBuffer buffer = r->vertex_buffers[i];
        r->garbage_buffers[garbage][num_buffers + i] = buffer;
    }

    r->num_garbage_buffers[garbage] += r->num_frames;

    {
        VkDeviceMemory memory = r->vertex_buffer_memory;
        r->garbage_memories[garbage][num_memories] = memory;
    }

    r->num_garbage_memories[garbage] += 1;

    return OWL_OK;
}

static int owl_renderer_garbage_push_index(struct owl_renderer *r)
{
    uint32_t i;
    uint32_t const capacity = OWL_ARRAY_SIZE(r->garbage_buffers[0]);
    uint32_t const garbage = r->garbage;
    uint32_t const num_buffers = r->num_garbage_buffers[garbage];
    uint32_t const num_memories = r->num_garbage_memories[garbage];

    if (capacity <= num_buffers + r->num_frames)
        return OWL_ERROR_NO_SPACE;

    if (capacity <= num_memories + 1)
        return OWL_ERROR_NO_SPACE;

    for (i = 0; i < r->num_frames; ++i) {
        VkBuffer buffer = r->index_buffers[i];
        r->garbage_buffers[garbage][num_buffers + i] = buffer;
    }

    r->num_garbage_buffers[garbage] += r->num_frames;

    {
        VkDeviceMemory memory = r->index_buffer_memory;
        r->garbage_memories[garbage][num_memories] = memory;
    }

    r->num_garbage_memories[garbage] += 1;

    return OWL_OK;
}

static int owl_renderer_garbage_push_uniform(struct owl_renderer *r)
{
    uint32_t i;
    uint32_t const capacity = OWL_ARRAY_SIZE(r->garbage_buffers[0]);
    uint32_t const garbage = r->garbage;
    uint32_t const num_buffers = r->num_garbage_buffers[garbage];
    uint32_t const num_memories = r->num_garbage_memories[garbage];
    uint32_t num_descriptor_sets = r->num_garbage_descriptor_sets[garbage];

    if (capacity <= num_buffers + r->num_frames)
        return OWL_ERROR_NO_SPACE;

    if (capacity <= num_memories + 1)
        return OWL_ERROR_NO_SPACE;

    if (capacity <= num_descriptor_sets + r->num_frames * 3)
        return OWL_ERROR_NO_SPACE;

    for (i = 0; i < r->num_frames; ++i) {
        VkBuffer buffer = r->uniform_buffers[i];
        r->garbage_buffers[garbage][num_buffers + i] = buffer;
    }
    r->num_garbage_buffers[garbage] += r->num_frames;

    {
        VkDeviceMemory memory = r->uniform_buffer_memory;
        r->garbage_memories[garbage][num_memories] = memory;
    }
    r->num_garbage_memories[garbage] += 1;

    for (i = 0; i < r->num_frames; ++i) {
        uint32_t const count = num_descriptor_sets;
        VkDescriptorSet descriptor_set;

        descriptor_set = r->uniform_pvm_descriptor_sets[i];
        r->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
    }
    num_descriptor_sets += r->num_frames;

    for (i = 0; i < OWL_NUM_IN_FLIGHT_FRAMES; ++i) {
        uint32_t const count = num_descriptor_sets;
        VkDescriptorSet descriptor_set;

        descriptor_set = r->uniform_model_descriptor_sets[i];
        r->garbage_descriptor_sets[garbage][count + i] = descriptor_set;
    }
    num_descriptor_sets += r->num_frames;
    r->num_garbage_descriptor_sets[garbage] = num_descriptor_sets;

    return OWL_OK;
}

static int owl_renderer_init_vertex_buffer(struct owl_renderer *r,
                                           uint64_t size)
{
    int32_t i;

    VkDevice const device = r->device;

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
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

        vk_result = vkCreateBuffer(device, &info, NULL, &r->vertex_buffers[i]);
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

        vkGetBufferMemoryRequirements(device, r->vertex_buffers[0],
                                      &requirements);

        r->vertex_buffer_alignment = requirements.alignment;
        aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
        r->vertex_buffer_aligned_size = aligned_size;

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = aligned_size * r->num_frames;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL,
                                     &r->vertex_buffer_memory);
        if (vk_result)
            goto error_destroy_buffers;

        for (j = 0; j < r->num_frames; ++j) {
            VkDeviceMemory memory = r->vertex_buffer_memory;
            VkBuffer buffer = r->vertex_buffers[j];
            vk_result = vkBindBufferMemory(device, buffer, memory,
                                           aligned_size * j);
            if (vk_result)
                goto error_free_memory;
        }

        vk_result = vkMapMemory(device, r->vertex_buffer_memory, 0, size, 0,
                                &r->vertex_buffer_data);
        if (vk_result)
            goto error_free_memory;
    }

    r->vertex_buffer_last_offset = 0;
    r->vertex_buffer_offset = 0;
    r->vertex_buffer_size = size;

    return OWL_OK;

error_free_memory:
    vkFreeMemory(device, r->vertex_buffer_memory, NULL);

    i = r->num_frames;

error_destroy_buffers:
    for (i = i - 1; i >= 0; --i)
        vkDestroyBuffer(device, r->vertex_buffers[i], NULL);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_vertex_buffer(struct owl_renderer *r)
{
    uint32_t i;
    VkDevice const device = r->device;

    vkFreeMemory(device, r->vertex_buffer_memory, NULL);

    for (i = 0; i < r->num_frames; ++i)
        vkDestroyBuffer(device, r->vertex_buffers[i], NULL);
}

static int owl_renderer_init_index_buffer(struct owl_renderer *r,
                                          uint64_t size)
{
    int32_t i;
    VkDevice const device = r->device;

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
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

        vk_result = vkCreateBuffer(device, &info, NULL, &r->index_buffers[i]);
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

        vkGetBufferMemoryRequirements(device, r->index_buffers[0],
                                      &requirements);

        r->index_buffer_alignment = requirements.alignment;
        aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
        r->index_buffer_aligned_size = aligned_size;

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = aligned_size * r->num_frames;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result =
                vkAllocateMemory(device, &info, NULL, &r->index_buffer_memory);
        if (vk_result)
            goto error_destroy_buffers;

        for (j = 0; j < r->num_frames; ++j) {
            VkDeviceMemory memory = r->index_buffer_memory;
            VkBuffer buffer = r->index_buffers[j];
            vk_result = vkBindBufferMemory(device, buffer, memory,
                                           aligned_size * j);
            if (vk_result)
                goto error_free_memory;
        }

        vk_result = vkMapMemory(device, r->index_buffer_memory, 0, size, 0,
                                &r->index_buffer_data);
        if (vk_result)
            goto error_free_memory;
    }

    r->index_buffer_last_offset = 0;
    r->index_buffer_offset = 0;
    r->index_buffer_size = size;

    return OWL_OK;

error_free_memory:
    vkFreeMemory(device, r->index_buffer_memory, NULL);

    i = r->num_frames;

error_destroy_buffers:
    for (i = i - 1; i >= 0; --i)
        vkDestroyBuffer(device, r->index_buffers[i], NULL);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_index_buffer(struct owl_renderer *r)
{
    uint32_t i;
    VkDevice const device = r->device;

    vkFreeMemory(device, r->index_buffer_memory, NULL);

    for (i = 0; i < r->num_frames; ++i)
        vkDestroyBuffer(device, r->index_buffers[i], NULL);
}

static int owl_renderer_init_uniform_buffer(struct owl_renderer *r,
                                            uint64_t size)
{
    int32_t i;
    VkDevice const device = r->device;

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
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

        vk_result =
                vkCreateBuffer(device, &info, NULL, &r->uniform_buffers[i]);
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

        vkGetBufferMemoryRequirements(device, r->uniform_buffers[0],
                                      &requirements);

        r->uniform_buffer_alignment = requirements.alignment;
        aligned_size = OWL_ALIGN_UP_2(size, requirements.alignment);
        r->uniform_buffer_aligned_size = aligned_size;

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = aligned_size * r->num_frames;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL,
                                     &r->uniform_buffer_memory);
        if (vk_result)
            goto error_destroy_buffers;

        for (j = 0; j < r->num_frames; ++j) {
            VkDeviceMemory memory = r->uniform_buffer_memory;
            VkBuffer buffer = r->uniform_buffers[j];
            vk_result = vkBindBufferMemory(device, buffer, memory,
                                           aligned_size * j);
            if (vk_result)
                goto error_free_memory;
        }

        vk_result = vkMapMemory(device, r->uniform_buffer_memory, 0, size, 0,
                                &r->uniform_buffer_data);
        if (vk_result)
            goto error_free_memory;
    }

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        {
            VkDescriptorSetLayout layouts[2];
            VkDescriptorSet descriptor_sets[2];
            VkDescriptorSetAllocateInfo info;
            VkResult vk_result = VK_SUCCESS;

            layouts[0] = r->common_uniform_descriptor_set_layout;
            layouts[1] = r->model_uniform_descriptor_set_layout;

            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            info.pNext = NULL;
            info.descriptorPool = r->descriptor_pool;
            info.descriptorSetCount = OWL_ARRAY_SIZE(layouts);
            info.pSetLayouts = layouts;

            vk_result =
                    vkAllocateDescriptorSets(device, &info, descriptor_sets);
            if (vk_result)
                goto error_free_descriptor_sets;

            r->uniform_pvm_descriptor_sets[i] = descriptor_sets[0];
            r->uniform_model_descriptor_sets[i] = descriptor_sets[1];
        }

        {
            VkDescriptorBufferInfo descriptors[2];
            VkWriteDescriptorSet writes[2];

            descriptors[0].buffer = r->uniform_buffers[i];
            descriptors[0].offset = 0;
            descriptors[0].range = sizeof(struct owl_common_uniform);

            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].pNext = NULL;
            writes[0].dstSet = r->uniform_pvm_descriptor_sets[i];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType =
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            writes[0].pImageInfo = NULL;
            writes[0].pBufferInfo = &descriptors[0];
            writes[0].pTexelBufferView = NULL;

            descriptors[1].buffer = r->uniform_buffers[i];
            descriptors[1].offset = 0;
            descriptors[1].range = sizeof(struct owl_model_uniform);

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].pNext = NULL;
            writes[1].dstSet = r->uniform_model_descriptor_sets[i];
            writes[1].dstBinding = 0;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType =
                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            writes[1].pImageInfo = NULL;
            writes[1].pBufferInfo = &descriptors[1];
            writes[1].pTexelBufferView = NULL;

            vkUpdateDescriptorSets(device, OWL_ARRAY_SIZE(writes), writes, 0,
                                   NULL);
        }
    }

    r->uniform_buffer_last_offset = 0;
    r->uniform_buffer_offset = 0;
    r->uniform_buffer_size = size;

    return OWL_OK;

error_free_descriptor_sets:
    for (i = i - 1; i >= 0; --i) {
        VkDescriptorSet descriptor_sets[3];

        descriptor_sets[0] = r->uniform_pvm_descriptor_sets[i];
        descriptor_sets[1] = r->uniform_model_descriptor_sets[i];

        vkFreeDescriptorSets(device, r->descriptor_pool,
                             OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
    }

    i = r->num_frames;

error_free_memory:
    vkFreeMemory(device, r->uniform_buffer_memory, NULL);

    i = r->num_frames;

error_destroy_buffers:
    for (i = i - 1; i >= 0; --i)
        vkDestroyBuffer(device, r->uniform_buffers[i], NULL);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_uniform_buffer(struct owl_renderer *r)
{
    uint32_t i;
    VkDevice const device = r->device;

    for (i = 0; i < r->num_frames; ++i) {
        VkDescriptorSet descriptor_sets[2];

        descriptor_sets[0] = r->uniform_pvm_descriptor_sets[i];
        descriptor_sets[1] = r->uniform_model_descriptor_sets[i];

        vkFreeDescriptorSets(device, r->descriptor_pool,
                             OWL_ARRAY_SIZE(descriptor_sets), descriptor_sets);
    }

    vkFreeMemory(device, r->uniform_buffer_memory, NULL);

    for (i = 0; i < r->num_frames; ++i)
        vkDestroyBuffer(device, r->uniform_buffers[i], NULL);
}

static int owl_renderer_init_frames(struct owl_renderer *r)
{
    int32_t i;
    VkDevice const device = r->device;

    r->frame = 0;

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        VkCommandPoolCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.queueFamilyIndex = r->graphics_family;

        vk_result = vkCreateCommandPool(device, &info, NULL,
                                        &r->submit_command_pools[i]);
        if (vk_result)
            goto error_destroy_submit_command_pools;
    }

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        VkCommandBufferAllocateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = NULL;
        info.commandPool = r->submit_command_pools[i];
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;

        vk_result = vkAllocateCommandBuffers(device, &info,
                                             &r->submit_command_buffers[i]);
        if (vk_result)
            goto error_free_submit_command_buffers;
    }

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        VkFenceCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vk_result =
                vkCreateFence(device, &info, NULL, &r->in_flight_fences[i]);
        if (vk_result)
            goto error_destroy_in_flight_fences;
    }

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        VkSemaphoreCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;

        vk_result = vkCreateSemaphore(device, &info, NULL,
                                      &r->acquire_semaphores[i]);
        if (vk_result)
            goto error_destroy_acquire_semaphores;
    }

    for (i = 0; i < (int32_t)r->num_frames; ++i) {
        VkSemaphoreCreateInfo info;
        VkResult vk_result = VK_SUCCESS;

        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;

        vk_result = vkCreateSemaphore(device, &info, NULL,
                                      &r->render_done_semaphores[i]);
        if (vk_result)
            goto error_destroy_render_done_semaphores;
    }

    return OWL_OK;

error_destroy_render_done_semaphores:
    for (i = i - 1; i > 0; --i) {
        VkSemaphore semaphore = r->render_done_semaphores[i];
        vkDestroySemaphore(device, semaphore, NULL);
    }

    i = r->num_frames;

error_destroy_acquire_semaphores:
    for (i = i - 1; i > 0; --i) {
        VkSemaphore semaphore = r->acquire_semaphores[i];
        vkDestroySemaphore(device, semaphore, NULL);
    }

    i = r->num_frames;

error_destroy_in_flight_fences:
    for (i = i - 1; i > 0; --i) {
        VkFence fence = r->in_flight_fences[i];
        vkDestroyFence(device, fence, NULL);
    }

    i = r->num_frames;

error_free_submit_command_buffers:
    for (i = i - 1; i > 0; --i) {
        VkCommandPool command_pool = r->submit_command_pools[i];
        VkCommandBuffer command_buffer = r->submit_command_buffers[i];
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

error_destroy_submit_command_pools:
    for (i = i - 1; i > 0; --i) {
        VkCommandPool command_pool = r->submit_command_pools[i];
        vkDestroyCommandPool(device, command_pool, NULL);
    }

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_frames(struct owl_renderer *r)
{
    uint32_t i;

    VkDevice const device = r->device;

    for (i = 0; i < r->num_frames; ++i) {
        VkSemaphore semaphore = r->render_done_semaphores[i];
        vkDestroySemaphore(device, semaphore, NULL);
    }

    i = r->num_frames;

    for (i = 0; i < r->num_frames; ++i) {
        VkSemaphore semaphore = r->acquire_semaphores[i];
        vkDestroySemaphore(device, semaphore, NULL);
    }

    i = r->num_frames;

    for (i = 0; i < r->num_frames; ++i) {
        VkFence fence = r->in_flight_fences[i];
        vkDestroyFence(device, fence, NULL);
    }

    i = r->num_frames;

    for (i = 0; i < r->num_frames; ++i) {
        VkCommandPool command_pool = r->submit_command_pools[i];
        VkCommandBuffer command_buffer = r->submit_command_buffers[i];
        vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
    }

    for (i = 0; i < r->num_frames; ++i) {
        VkCommandPool command_pool = r->submit_command_pools[i];
        vkDestroyCommandPool(device, command_pool, NULL);
    }
}

static int owl_renderer_init_samplers(struct owl_renderer *r)
{
    VkSamplerCreateInfo info;
    VkResult vk_result = VK_SUCCESS;
    VkDevice const device = r->device;

    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.pNext = NULL;
    info.flags = 0;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
#if 0
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#else
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
#endif
    info.mipLodBias = 0.0F;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = 16.0F;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.minLod = 0.0F;
    info.maxLod = VK_LOD_CLAMP_NONE;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;

    vk_result = vkCreateSampler(device, &info, NULL, &r->linear_sampler);
    if (vk_result)
        return OWL_ERROR_FATAL;

    return OWL_OK;
}

static void owl_renderer_deinit_samplers(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroySampler(device, r->linear_sampler, NULL);
}

/* TODO(samuel): cleanup */

#define OWL_IRRADIANCE_MAP 0
#define OWL_PREFILTERED_MAP 1
#define OWL_NUM_ENVIRONMENT_MAPS 2

struct owl_renderer_irradiance_push_constant {
    owl_m4 mvp;
    float delta_phi;
    float delta_theta;
};

struct owl_renderer_prefiltered_push_constant {
    owl_m4 mvp;
    float roughness;
    uint32_t samples;
};

static int owl_renderer_init_filter_maps(struct owl_renderer *r)
{
    int32_t i;
    VkImage *images[OWL_NUM_ENVIRONMENT_MAPS];
    VkDeviceMemory *memories[OWL_NUM_ENVIRONMENT_MAPS];
    VkImageView *image_views[OWL_NUM_ENVIRONMENT_MAPS];
    VkFormat formats[OWL_NUM_ENVIRONMENT_MAPS];
    uint32_t dimensions[OWL_NUM_ENVIRONMENT_MAPS];
    VkDeviceSize push_constant_ranges[OWL_NUM_ENVIRONMENT_MAPS];

    static uint32_t const vertex_shader_source[] = {
#include "owl_environment.vert.spv.u32"
    };

    static uint32_t const irradiance_fragment_shader_source[] = {
#include "owl_irradiance.frag.spv.u32"
    };

    static uint32_t const prefilter_fragment_shader_source[] = {
#include "owl_prefilter.frag.spv.u32"
    };

    VkRenderPass offscreen_pass = VK_NULL_HANDLE;
    VkImage offscreen_image = VK_NULL_HANDLE;
    VkDeviceMemory offscreen_memory = VK_NULL_HANDLE;
    VkImageView offscreen_image_view = VK_NULL_HANDLE;
    VkFramebuffer offscreen_framebuffer = VK_NULL_HANDLE;
    VkShaderModule offscreen_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule offscreen_fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout offscreen_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline offscreen_pipeline = VK_NULL_HANDLE;

    int ret = OWL_OK;
    VkDevice const device = r->device;

    formats[OWL_IRRADIANCE_MAP] = VK_FORMAT_R32G32B32A32_SFLOAT;
    formats[OWL_PREFILTERED_MAP] = VK_FORMAT_R16G16B16A16_SFLOAT;

    dimensions[OWL_IRRADIANCE_MAP] = 64;
    dimensions[OWL_PREFILTERED_MAP] = 512;

    images[OWL_IRRADIANCE_MAP] = &r->irradiance_map_image;
    images[OWL_PREFILTERED_MAP] = &r->prefiltered_map_image;

    memories[OWL_IRRADIANCE_MAP] = &r->irradiance_map_memory;
    memories[OWL_PREFILTERED_MAP] = &r->prefiltered_map_memory;

    image_views[OWL_IRRADIANCE_MAP] = &r->irradiance_map_image_view;
    image_views[OWL_PREFILTERED_MAP] = &r->prefiltered_map_image_view;

    push_constant_ranges[OWL_IRRADIANCE_MAP] =
            sizeof(struct owl_renderer_irradiance_push_constant);
    push_constant_ranges[OWL_PREFILTERED_MAP] =
            sizeof(struct owl_renderer_prefiltered_push_constant);

    r->prefiltered_map_image = VK_NULL_HANDLE;
    r->prefiltered_map_memory = VK_NULL_HANDLE;
    r->prefiltered_map_image_view = VK_NULL_HANDLE;

    r->irradiance_map_image = VK_NULL_HANDLE;
    r->irradiance_map_memory = VK_NULL_HANDLE;
    r->irradiance_map_image_view = VK_NULL_HANDLE;

    for (i = 0; i < OWL_NUM_ENVIRONMENT_MAPS; ++i) {
        uint32_t j;
        VkViewport viewport;
        VkRect2D scissor;
        owl_m4 matrices[6];

        VkFormat const format = formats[i];
        uint32_t const dimension = dimensions[i];
        uint32_t const mips =
                owl_texture_calculate_mipmaps(dimension, dimension);

        viewport.x = 0;
        viewport.y = 0;
        viewport.width = dimension;
        viewport.height = dimension;
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = dimension;
        scissor.extent.height = dimension;

        {
            VkImageCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.pNext = NULL;
            info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = formats[i];
            info.extent.width = dimension;
            info.extent.height = dimension;
            info.extent.depth = 1;
            info.mipLevels = mips;
            info.arrayLayers = 6;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = 0;
            info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = NULL;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            vk_result = vkCreateImage(device, &info, NULL, images[i]);
            if (vk_result)
                goto error;
        }

        {
            VkMemoryPropertyFlagBits properties;
            VkMemoryRequirements requirements;
            VkMemoryAllocateInfo info;
            VkResult vk_result;

            properties = 0;
            properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            vkGetImageMemoryRequirements(device, *images[i], &requirements);

            info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            info.pNext = NULL;
            info.allocationSize = requirements.size;
            info.memoryTypeIndex = owl_renderer_find_memory_type(
                    r, requirements.memoryTypeBits, properties);

            vk_result = vkAllocateMemory(device, &info, NULL, memories[i]);
            if (vk_result)
                goto error;

            vk_result = vkBindImageMemory(device, *images[i], *memories[i], 0);
            if (vk_result)
                goto error;
        }

        {
            VkImageViewCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.image = *images[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            info.format = format;
            info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = mips;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 6;

            vk_result = vkCreateImageView(device, &info, NULL, image_views[i]);
            if (vk_result)
                goto error;
        }

        {
            VkAttachmentReference color_reference;
            VkAttachmentDescription attachment;
            VkSubpassDescription subpass;
            VkSubpassDependency dependency;
            VkRenderPassCreateInfo info;
            VkResult vk_result;

            color_reference.attachment = 0;
            color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachment.flags = 0;
            attachment.format = format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            subpass.flags = 0;
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.inputAttachmentCount = 0;
            subpass.pInputAttachments = NULL;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &color_reference;
            subpass.pResolveAttachments = NULL;
            subpass.pDepthStencilAttachment = NULL;
            subpass.preserveAttachmentCount = 0;
            subpass.pPreserveAttachments = NULL;

            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = 0;
            dependency.srcStageMask |=
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask = 0;
            dependency.dstStageMask |=
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask |=
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = 0;
            dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependency.dstAccessMask |=
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dependencyFlags = 0;

            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;

            vk_result =
                    vkCreateRenderPass(device, &info, NULL, &offscreen_pass);
            if (vk_result)
                goto error;
        }

        {
            VkImageCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.pNext = NULL;
            info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = formats[i];
            info.extent.width = dimension;
            info.extent.height = dimension;
            info.extent.depth = 1;
            info.mipLevels = 1;
            info.arrayLayers = 6;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = 0;
            info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.queueFamilyIndexCount = 0;
            info.pQueueFamilyIndices = NULL;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            vk_result = vkCreateImage(device, &info, NULL, &offscreen_image);
            if (vk_result)
                goto error;
        }

        {
            VkMemoryPropertyFlagBits properties;
            VkMemoryRequirements requirements;
            VkMemoryAllocateInfo info;
            VkResult vk_result;

            properties = 0;
            properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            vkGetImageMemoryRequirements(device, offscreen_image,
                                         &requirements);

            info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            info.pNext = NULL;
            info.allocationSize = requirements.size;
            info.memoryTypeIndex = owl_renderer_find_memory_type(
                    r, requirements.memoryTypeBits, properties);

            vk_result =
                    vkAllocateMemory(device, &info, NULL, &offscreen_memory);
            if (vk_result)
                goto error;

            vk_result = vkBindImageMemory(device, offscreen_image,
                                          offscreen_memory, 0);
            if (vk_result)
                goto error;
        }

        {
            VkImageViewCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.image = offscreen_image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = format;
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
                                          &offscreen_image_view);
            if (vk_result)
                goto error;
        }

        {
            VkFramebufferCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.renderPass = offscreen_pass;
            info.attachmentCount = 1;
            info.pAttachments = &offscreen_image_view;
            info.width = dimension;
            info.height = dimension;
            info.layers = 1;

            vk_result = vkCreateFramebuffer(device, &info, NULL,
                                            &offscreen_framebuffer);
            if (vk_result)
                goto error;
        }

        /* TODO(samuel): why does he change the layout manualy instead of leaving
     * it to the render pass?
https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/src/main.cpp#L1288
     */
#if 0
    ret = owl_renderer_begin_im_command_buffer(r);
    if (ret)
      goto error;

    {

    }

    ret = owl_renderer_end_im_command_buffer(r);
    if (ret)
      goto error;
#endif

        {
            VkPushConstantRange range;
            VkPipelineLayoutCreateInfo info;
            VkResult vk_result;

            range.stageFlags = 0;
            range.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
            range.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            range.offset = 0;
            range.size = push_constant_ranges[i];

            info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.setLayoutCount = 1;
            info.pSetLayouts = &r->common_texture_descriptor_set_layout;
            info.pushConstantRangeCount = 1;
            info.pPushConstantRanges = &range;

            vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                               &offscreen_pipeline_layout);
            if (vk_result)
                goto error;
        }

        {
            VkShaderModuleCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            info.codeSize = sizeof(vertex_shader_source);
            info.pCode = vertex_shader_source;

            vk_result = vkCreateShaderModule(device, &info, NULL,
                                             &offscreen_vertex_shader);
            if (vk_result)
                goto error;
        }

        {
            VkShaderModuleCreateInfo info;
            VkResult vk_result;

            info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.pNext = NULL;
            info.flags = 0;
            if (OWL_IRRADIANCE_MAP == i) {
                info.codeSize = sizeof(irradiance_fragment_shader_source);
                info.pCode = irradiance_fragment_shader_source;
            } else if (OWL_PREFILTERED_MAP == i) {
                info.codeSize = sizeof(prefilter_fragment_shader_source);
                info.pCode = prefilter_fragment_shader_source;
            } else {
                OWL_ASSERT(0);
                goto error;
            }

            vk_result = vkCreateShaderModule(device, &info, NULL,
                                             &offscreen_fragment_shader);
            if (vk_result)
                goto error;
        }

        {
            VkVertexInputBindingDescription vertex_binding;
            VkVertexInputAttributeDescription vertex_attribute;
            VkPipelineVertexInputStateCreateInfo vertex_input;
            VkPipelineInputAssemblyStateCreateInfo input_assembly;
            VkPipelineViewportStateCreateInfo viewport_state;
            VkPipelineRasterizationStateCreateInfo rasterization;
            VkPipelineMultisampleStateCreateInfo multisample;
            VkPipelineColorBlendAttachmentState color_attachment;
            VkPipelineColorBlendStateCreateInfo color;
            VkPipelineDepthStencilStateCreateInfo depth;
            VkDynamicState dynamic[2];
            VkPipelineDynamicStateCreateInfo dynamic_state;
            VkPipelineShaderStageCreateInfo stages[2];
            VkGraphicsPipelineCreateInfo info;
            VkResult vk_result;

            vertex_binding.binding = 0;
            /* TODO(samuel): remove dependance on the skybox vertex */
            vertex_binding.stride = sizeof(struct owl_skybox_vertex);
            vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            vertex_attribute.location = 0;
            vertex_attribute.binding = 0;
            vertex_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
            vertex_attribute.offset =
                    offsetof(struct owl_skybox_vertex, position);

            vertex_input.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertex_input.pNext = NULL;
            vertex_input.flags = 0;
            vertex_input.vertexBindingDescriptionCount = 1;
            vertex_input.pVertexBindingDescriptions = &vertex_binding;
            vertex_input.vertexAttributeDescriptionCount = 1;
            vertex_input.pVertexAttributeDescriptions = &vertex_attribute;

            input_assembly.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            input_assembly.pNext = NULL;
            input_assembly.flags = 0;
            input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            input_assembly.primitiveRestartEnable = VK_FALSE;

            viewport.x = 0;
            viewport.y = 0;
            viewport.width = dimension;
            viewport.height = dimension;
            viewport.minDepth = 0.0F;
            viewport.maxDepth = 1.0F;

            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = dimension;
            scissor.extent.height = dimension;

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
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisample.sampleShadingEnable = VK_FALSE;
            multisample.minSampleShading = 0.0F;
            multisample.pSampleMask = NULL;
            multisample.alphaToCoverageEnable = VK_FALSE;
            multisample.alphaToOneEnable = VK_FALSE;

            color_attachment.blendEnable = VK_FALSE;
            color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
            color_attachment.colorWriteMask = 0;
            color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
            color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
            color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
            color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

            color.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            color.pNext = NULL;
            color.flags = 0;
            color.logicOpEnable = VK_FALSE;
            color.logicOp = VK_LOGIC_OP_CLEAR;
            color.attachmentCount = 1;
            color.pAttachments = &color_attachment;
            color.blendConstants[0] = 0.0F;
            color.blendConstants[1] = 0.0F;
            color.blendConstants[2] = 0.0F;
            color.blendConstants[3] = 0.0F;

            depth.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depth.pNext = NULL;
            depth.flags = 0;
            depth.depthTestEnable = VK_FALSE;
            depth.depthWriteEnable = VK_FALSE;
            depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depth.depthBoundsTestEnable = VK_FALSE;
            depth.stencilTestEnable = VK_FALSE;
            depth.front.failOp = VK_STENCIL_OP_KEEP;
            depth.front.passOp = VK_STENCIL_OP_KEEP;
            depth.front.depthFailOp = VK_STENCIL_OP_KEEP;
            depth.front.compareOp = VK_COMPARE_OP_NEVER;
            depth.front.compareMask = 0;
            depth.front.writeMask = 0;
            depth.front.reference = 0;
            depth.back.failOp = VK_STENCIL_OP_KEEP;
            depth.back.passOp = VK_STENCIL_OP_KEEP;
            depth.back.depthFailOp = VK_STENCIL_OP_KEEP;
            depth.back.compareOp = VK_COMPARE_OP_ALWAYS;
            depth.back.compareMask = 0;
            depth.back.writeMask = 0;
            depth.back.reference = 0;
            depth.minDepthBounds = 0.0F;
            depth.maxDepthBounds = 0.0F;

            stages[0].sType =
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].pNext = NULL;
            stages[0].flags = 0;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = offscreen_vertex_shader;
            stages[0].pName = "main";
            stages[0].pSpecializationInfo = NULL;

            stages[1].sType =
                    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].pNext = NULL;
            stages[1].flags = 0;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = offscreen_fragment_shader;
            stages[1].pName = "main";
            stages[1].pSpecializationInfo = NULL;

            dynamic[0] = VK_DYNAMIC_STATE_VIEWPORT;
            dynamic[1] = VK_DYNAMIC_STATE_SCISSOR;

            dynamic_state.sType =
                    VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic_state.pNext = NULL;
            dynamic_state.flags = 0;
            dynamic_state.dynamicStateCount = OWL_ARRAY_SIZE(dynamic);
            dynamic_state.pDynamicStates = dynamic;

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
            info.pDynamicState = &dynamic_state;
            info.layout = offscreen_pipeline_layout;
            info.renderPass = offscreen_pass;
            info.subpass = 0;
            info.basePipelineHandle = VK_NULL_HANDLE;
            info.basePipelineIndex = -1;

            vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                                                  &info, NULL,
                                                  &offscreen_pipeline);
            if (vk_result)
                goto error;
        }

        {
            float angle;
            owl_v3 axis;
            owl_m4 tmp;

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(90.0F);
            axis[0] = 0.0F;
            axis[1] = 1.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, tmp);
            angle = OWL_DEGREES_AS_RADIANS(180.0F);
            axis[0] = 1.0F;
            axis[1] = 0.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[0]);

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(-90.0F);
            axis[0] = 0.0F;
            axis[1] = 1.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, tmp);
            angle = OWL_DEGREES_AS_RADIANS(180.0F);
            axis[0] = 1.0F;
            axis[1] = 0.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[1]);

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(-90.0F);
            axis[0] = 1.0F;
            axis[1] = 0.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[2]);

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(90.0F);
            axis[0] = 1.0F;
            axis[1] = 0.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[3]);

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(180.0F);
            axis[0] = 1.0F;
            axis[1] = 0.0F;
            axis[2] = 0.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[4]);

            OWL_M4_IDENTITY(tmp);
            angle = OWL_DEGREES_AS_RADIANS(180.0F);
            axis[0] = 0.0F;
            axis[1] = 0.0F;
            axis[2] = 1.0F;
            owl_m4_rotate(tmp, angle, axis, matrices[5]);
        }

        ret = owl_renderer_begin_im_command_buffer(r);
        if (ret)
            goto error;

        {
            VkImageMemoryBarrier barrier;
            VkCommandBuffer const command_buffer = r->im_command_buffer;

            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_NONE;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *images[i];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mips;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 6;

            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                 NULL, 0, NULL, 1, &barrier);
        }

        ret = owl_renderer_end_im_command_buffer(r);
        if (ret)
            goto error;

        for (j = 0; j < mips; ++j) {
            uint32_t k;
            for (k = 0; k < 6; ++k) {
                VkCommandBuffer command_buffer;

                ret = owl_renderer_begin_im_command_buffer(r);
                if (ret)
                    goto error;

                command_buffer = r->im_command_buffer;

                {
                    viewport.width = dimension * OWL_POW(0.5, (double)j);
                    viewport.height = viewport.width;

                    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
                    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
                }

                {
                    VkClearValue clear_value;
                    VkRenderPassBeginInfo info;

                    clear_value.color.float32[0] = 0.0F;
                    clear_value.color.float32[1] = 0.0F;
                    clear_value.color.float32[2] = 0.2F;
                    clear_value.color.float32[3] = 0.0F;

                    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    info.pNext = NULL;
                    info.renderPass = offscreen_pass;
                    info.framebuffer = offscreen_framebuffer;
                    info.renderArea.offset.x = 0;
                    info.renderArea.offset.y = 0;
                    info.renderArea.extent.width = dimension;
                    info.renderArea.extent.height = dimension;
                    info.clearValueCount = 1;
                    info.pClearValues = &clear_value;

                    vkCmdBeginRenderPass(command_buffer, &info,
                                         VK_SUBPASS_CONTENTS_INLINE);
                }

                {
                    vkCmdBindPipeline(command_buffer,
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      offscreen_pipeline);
                }

                if (OWL_IRRADIANCE_MAP == i) {
                    float fov;
                    float ratio;
                    float near;
                    float far;
                    owl_m4 perspective;
                    struct owl_renderer_irradiance_push_constant push_constant;

                    fov = OWL_PI / 2.0F;
                    ratio = 1.0F;
                    near = 0.1F;
                    far = 512.0F;
                    owl_m4_perspective(fov, ratio, near, far, perspective);
                    owl_m4_multiply(perspective, matrices[k],
                                    push_constant.mvp);

                    push_constant.delta_phi = 2.0F * OWL_PI / 180.0F;
                    push_constant.delta_theta = 0.5F * OWL_PI / 64.0F;

                    vkCmdPushConstants(command_buffer,
                                       offscreen_pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT |
                                               VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(push_constant),
                                       &push_constant);
                } else if (OWL_PREFILTERED_MAP == i) {
                    float fov;
                    float ratio;
                    float near;
                    float far;
                    owl_m4 perspective;
                    struct owl_renderer_prefiltered_push_constant push_constant;

                    fov = OWL_PI / 2.0F;
                    ratio = 1.0F;
                    near = 0.1F;
                    far = 512.0F;
                    owl_m4_perspective(fov, ratio, near, far, perspective);
                    owl_m4_multiply(perspective, matrices[k],
                                    push_constant.mvp);

                    push_constant.roughness = (float)j / (float)(mips - 1);
                    push_constant.samples = 32;

                    vkCmdPushConstants(command_buffer,
                                       offscreen_pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT |
                                               VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0, sizeof(push_constant),
                                       &push_constant);
                }

                {
                    vkCmdBindDescriptorSets(command_buffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            offscreen_pipeline_layout, 0, 1,
                                            &r->skybox.descriptor_set, 0,
                                            NULL);
                }

                {
                    struct owl_skybox_vertex *mapped_vertices;
                    struct owl_renderer_vertex_allocation vertex_allocation;
                    uint32_t *mapped_indices;
                    struct owl_renderer_index_allocation index_allocation;
                    /*
           *    4----5
           *  / |  / |
           * 0----1  |
           * |  | |  |
           * |  6-|--7
           * | /  | /
           * 2----3
           */
                    static struct owl_skybox_vertex const vertices[] = {
                        { -1.0F, -1.0F, -1.0F }, /* 0 */
                        { 1.0F, -1.0F, -1.0F }, /* 1 */
                        { -1.0F, 1.0F, -1.0F }, /* 2 */
                        { 1.0F, 1.0F, -1.0F }, /* 3 */
                        { -1.0F, -1.0F, 1.0F }, /* 4 */
                        { 1.0F, -1.0F, 1.0F }, /* 5 */
                        { -1.0F, 1.0F, 1.0F }, /* 6 */
                        { 1.0F, 1.0F, 1.0F }
                    }; /* 7 */
                    static uint32_t const indices[] = {
                        2, 3, 1, 1, 0, 2, /* face 0 .....*/
                        3, 7, 5, 5, 1, 3, /* face 1 */
                        6, 2, 0, 0, 4, 6, /* face 2 */
                        7, 6, 4, 4, 5, 7, /* face 3 */
                        3, 2, 6, 6, 7, 3, /* face 4 */
                        4, 0, 1, 1, 5, 4
                    }; /* face 5 */

                    mapped_vertices = owl_renderer_vertex_allocate(
                            r, sizeof(vertices), &vertex_allocation);
                    if (!mapped_vertices)
                        goto error;

                    mapped_indices = owl_renderer_index_allocate(
                            r, sizeof(indices), &index_allocation);
                    if (!mapped_indices)
                        goto error;

                    OWL_MEMCPY(mapped_vertices, vertices, sizeof(vertices));
                    OWL_MEMCPY(mapped_indices, indices, sizeof(indices));

                    vkCmdBindVertexBuffers(command_buffer, 0, 1,
                                           &vertex_allocation.buffer,
                                           &vertex_allocation.offset);

                    vkCmdBindIndexBuffer(command_buffer,
                                         index_allocation.buffer,
                                         index_allocation.offset,
                                         VK_INDEX_TYPE_UINT32);

                    vkCmdDrawIndexed(command_buffer, OWL_ARRAY_SIZE(indices),
                                     1, 0, 0, 0);
                }

                {
                    /* */
                    vkCmdEndRenderPass(command_buffer);
                }

                {
                    VkImageMemoryBarrier barrier;

                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = NULL;
                    barrier.srcAccessMask =
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    barrier.oldLayout =
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = offscreen_image;
                    barrier.subresourceRange.aspectMask =
                            VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = 1;

                    vkCmdPipelineBarrier(command_buffer,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                                         0, NULL, 0, NULL, 1, &barrier);
                }

                {
                    /**/
                    VkImageCopy copy;

                    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    copy.srcSubresource.mipLevel = 0;
                    copy.srcSubresource.baseArrayLayer = 0;
                    copy.srcSubresource.layerCount = 1;
                    copy.srcOffset.x = 0;
                    copy.srcOffset.y = 0;
                    copy.srcOffset.z = 0;
                    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    copy.dstSubresource.mipLevel = j;
                    copy.dstSubresource.baseArrayLayer = k;
                    copy.dstSubresource.layerCount = 1;
                    copy.dstOffset.x = 0;
                    copy.dstOffset.y = 0;
                    copy.dstOffset.z = 0;
                    copy.extent.width = viewport.width;
                    copy.extent.height = viewport.height;
                    copy.extent.depth = 1;

                    vkCmdCopyImage(command_buffer, offscreen_image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   *images[i],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copy);
                }

                {
                    VkImageMemoryBarrier barrier;

                    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrier.pNext = NULL;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    barrier.dstAccessMask =
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrier.newLayout =
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barrier.image = offscreen_image;
                    barrier.subresourceRange.aspectMask =
                            VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.baseMipLevel = 0;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.baseArrayLayer = 0;
                    barrier.subresourceRange.layerCount = 1;

                    vkCmdPipelineBarrier(command_buffer,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                                         0, NULL, 0, NULL, 1, &barrier);
                }

                ret = owl_renderer_end_im_command_buffer(r);
                if (ret)
                    goto error;

                owl_renderer_vertex_clear_offset(r);
                owl_renderer_index_clear_offset(r);
            }
        }

        ret = owl_renderer_begin_im_command_buffer(r);
        if (ret)
            goto error;

        {
            VkImageMemoryBarrier barrier;
            VkCommandBuffer const command_buffer = r->im_command_buffer;

            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = NULL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
/* TODO(samuel): verify
https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/src/main.cpp#L1619
*/
#if 1
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
#else
            barrier.dstAccessMask = 0;
            barrier.dstAccessMask |= VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
#endif
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *images[i];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mips;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 6;

            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                                 NULL, 0, NULL, 1, &barrier);
        }
        ret = owl_renderer_end_im_command_buffer(r);
        if (ret)
            goto error;

        if (OWL_PREFILTERED_MAP == i)
            r->prefiltered_map_mipmaps = mips;

        vkDestroyPipeline(device, offscreen_pipeline, NULL);
        offscreen_pipeline = VK_NULL_HANDLE;

        vkDestroyPipelineLayout(device, offscreen_pipeline_layout, NULL);
        offscreen_pipeline_layout = VK_NULL_HANDLE;

        vkDestroyShaderModule(device, offscreen_fragment_shader, NULL);
        offscreen_fragment_shader = VK_NULL_HANDLE;

        vkDestroyShaderModule(device, offscreen_vertex_shader, NULL);
        offscreen_vertex_shader = VK_NULL_HANDLE;

        vkDestroyFramebuffer(device, offscreen_framebuffer, NULL);
        offscreen_framebuffer = VK_NULL_HANDLE;

        vkDestroyImageView(device, offscreen_image_view, NULL);
        offscreen_image_view = VK_NULL_HANDLE;

        vkFreeMemory(device, offscreen_memory, NULL);
        offscreen_memory = VK_NULL_HANDLE;

        vkDestroyImage(device, offscreen_image, NULL);
        offscreen_image = VK_NULL_HANDLE;

        vkDestroyRenderPass(device, offscreen_pass, NULL);
        offscreen_pass = VK_NULL_HANDLE;
    }

    return OWL_OK;

error:
    owl_renderer_vertex_clear_offset(r);
    owl_renderer_index_clear_offset(r);

    if (offscreen_pipeline)
        vkDestroyPipeline(device, offscreen_pipeline, NULL);

    if (offscreen_pipeline_layout)
        vkDestroyPipelineLayout(device, offscreen_pipeline_layout, NULL);

    if (offscreen_fragment_shader)
        vkDestroyShaderModule(device, offscreen_fragment_shader, NULL);

    if (offscreen_vertex_shader)
        vkDestroyShaderModule(device, offscreen_vertex_shader, NULL);

    if (offscreen_framebuffer)
        vkDestroyFramebuffer(device, offscreen_framebuffer, NULL);

    if (offscreen_image_view)
        vkDestroyImageView(device, offscreen_image_view, NULL);

    if (offscreen_memory)
        vkFreeMemory(device, offscreen_memory, NULL);

    if (offscreen_image)
        vkDestroyImage(device, offscreen_image, NULL);

    if (offscreen_pass)
        vkDestroyRenderPass(device, offscreen_pass, NULL);

    if (r->prefiltered_map_image_view)
        vkDestroyImageView(device, r->prefiltered_map_image_view, NULL);

    if (r->prefiltered_map_memory)
        vkFreeMemory(device, r->prefiltered_map_memory, NULL);

    if (r->prefiltered_map_image)
        vkDestroyImage(device, r->prefiltered_map_image, NULL);

    if (r->irradiance_map_image_view)
        vkDestroyImageView(device, r->irradiance_map_image_view, NULL);

    if (r->irradiance_map_memory)
        vkFreeMemory(device, r->irradiance_map_memory, NULL);

    if (r->irradiance_map_image)
        vkDestroyImage(device, r->irradiance_map_image, NULL);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_filter_maps(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyImageView(device, r->prefiltered_map_image_view, NULL);
    vkFreeMemory(device, r->prefiltered_map_memory, NULL);
    vkDestroyImage(device, r->prefiltered_map_image, NULL);
    vkDestroyImageView(device, r->irradiance_map_image_view, NULL);
    vkFreeMemory(device, r->irradiance_map_memory, NULL);
    vkDestroyImage(device, r->irradiance_map_image, NULL);
}

static int owl_renderer_init_brdflut(struct owl_renderer *r)
{
    int ret = OWL_OK;
    VkFormat const format = VK_FORMAT_R16G16_SFLOAT;
    uint32_t const dimension = 512;

    static uint32_t const vertex_shader_source[] = {
#include "owl_brdflut.vert.spv.u32"
    };

    static uint32_t const fragment_shader_source[] = {
#include "owl_brdflut.frag.spv.u32"
    };

    VkRenderPass offscreen_pass = VK_NULL_HANDLE;
    VkFramebuffer offscreen_framebuffer = VK_NULL_HANDLE;
    VkShaderModule offscreen_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule offscreen_fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout offscreen_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline offscreen_pipeline = VK_NULL_HANDLE;
    VkDevice const device = r->device;

    {
        VkImageCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent.width = dimension;
        info.extent.height = dimension;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = 0;
        info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = NULL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vk_result = vkCreateImage(device, &info, NULL, &r->brdflut_map_image);
        if (vk_result)
            goto error;
    }

    {
        VkMemoryPropertyFlagBits properties;
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo info;
        VkResult vk_result;

        properties = 0;
        properties |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vkGetImageMemoryRequirements(device, r->brdflut_map_image,
                                     &requirements);

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = requirements.size;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result =
                vkAllocateMemory(device, &info, NULL, &r->brdflut_map_memory);
        if (vk_result)
            goto error;

        vk_result = vkBindImageMemory(device, r->brdflut_map_image,
                                      r->brdflut_map_memory, 0);
        if (vk_result)
            goto error;
    }

    {
        VkImageViewCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.image = r->brdflut_map_image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
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
                                      &r->brdflut_map_image_view);
        if (vk_result)
            goto error;
    }

    {
        VkAttachmentReference color_reference;
        VkAttachmentDescription attachment;
        VkSubpassDescription subpass;
        VkSubpassDependency dependency;
        VkRenderPassCreateInfo info;
        VkResult vk_result;

        color_reference.attachment = 0;
        color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachment.flags = 0;
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = NULL;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_reference;
        subpass.pResolveAttachments = NULL;
        subpass.pDepthStencilAttachment = NULL;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;

        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = 0;
        dependency.srcStageMask |=
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = 0;
        dependency.dstStageMask |=
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = 0;
        dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask |=
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;

        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        vk_result = vkCreateRenderPass(device, &info, NULL, &offscreen_pass);
        if (vk_result)
            goto error;
    }

    {
        VkFramebufferCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.renderPass = offscreen_pass;
        info.attachmentCount = 1;
        info.pAttachments = &r->brdflut_map_image_view;
        info.width = dimension;
        info.height = dimension;
        info.layers = 1;

        vk_result = vkCreateFramebuffer(device, &info, NULL,
                                        &offscreen_framebuffer);
        if (vk_result)
            goto error;
    }

    {
        VkPipelineLayoutCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.setLayoutCount = 0;
        info.pSetLayouts = NULL;
        info.pushConstantRangeCount = 0;
        info.pPushConstantRanges = NULL;

        vk_result = vkCreatePipelineLayout(device, &info, NULL,
                                           &offscreen_pipeline_layout);
        if (vk_result)
            goto error;
    }

    {
        VkShaderModuleCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.codeSize = sizeof(vertex_shader_source);
        info.pCode = vertex_shader_source;

        vk_result = vkCreateShaderModule(device, &info, NULL,
                                         &offscreen_vertex_shader);
        if (vk_result)
            goto error;
    }

    {
        VkShaderModuleCreateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.pNext = NULL;
        info.flags = 0;
        info.codeSize = sizeof(fragment_shader_source);
        info.pCode = fragment_shader_source;

        vk_result = vkCreateShaderModule(device, &info, NULL,
                                         &offscreen_fragment_shader);
        if (vk_result)
            goto error;
    }

    {
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

        vertex_input.sType =
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.pNext = NULL;
        vertex_input.flags = 0;
        vertex_input.vertexBindingDescriptionCount = 0;
        vertex_input.pVertexBindingDescriptions = NULL;
        vertex_input.vertexAttributeDescriptionCount = 0;
        vertex_input.pVertexAttributeDescriptions = NULL;

        input_assembly.sType =
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.pNext = NULL;
        input_assembly.flags = 0;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        viewport.x = 0;
        viewport.y = 0;
        viewport.width = dimension;
        viewport.height = dimension;
        viewport.minDepth = 0.0F;
        viewport.maxDepth = 1.0F;

        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = dimension;
        scissor.extent.height = dimension;

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
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample.sampleShadingEnable = VK_FALSE;
        multisample.minSampleShading = 0.0F;
        multisample.pSampleMask = NULL;
        multisample.alphaToCoverageEnable = VK_FALSE;
        multisample.alphaToOneEnable = VK_FALSE;

        color_attachment.blendEnable = VK_FALSE;
        color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        color_attachment.colorWriteMask = 0;
        color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        color_attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

        color.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color.pNext = NULL;
        color.flags = 0;
        color.logicOpEnable = VK_FALSE;
        color.logicOp = VK_LOGIC_OP_CLEAR;
        color.attachmentCount = 1;
        color.pAttachments = &color_attachment;
        color.blendConstants[0] = 0.0F;
        color.blendConstants[1] = 0.0F;
        color.blendConstants[2] = 0.0F;
        color.blendConstants[3] = 0.0F;

        depth.sType =
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.pNext = NULL;
        depth.flags = 0;
        depth.depthTestEnable = VK_FALSE;
        depth.depthWriteEnable = VK_FALSE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth.depthBoundsTestEnable = VK_FALSE;
        depth.stencilTestEnable = VK_FALSE;
        depth.front.failOp = VK_STENCIL_OP_KEEP;
        depth.front.passOp = VK_STENCIL_OP_KEEP;
        depth.front.depthFailOp = VK_STENCIL_OP_KEEP;
        depth.front.compareOp = VK_COMPARE_OP_NEVER;
        depth.front.compareMask = 0;
        depth.front.writeMask = 0;
        depth.front.reference = 0;
        depth.back.failOp = VK_STENCIL_OP_KEEP;
        depth.back.passOp = VK_STENCIL_OP_KEEP;
        depth.back.depthFailOp = VK_STENCIL_OP_KEEP;
        depth.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depth.back.compareMask = 0;
        depth.back.writeMask = 0;
        depth.back.reference = 0;
        depth.minDepthBounds = 0.0F;
        depth.maxDepthBounds = 0.0F;

        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].pNext = NULL;
        stages[0].flags = 0;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = offscreen_vertex_shader;
        stages[0].pName = "main";
        stages[0].pSpecializationInfo = NULL;

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].pNext = NULL;
        stages[1].flags = 0;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = offscreen_fragment_shader;
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
        info.layout = offscreen_pipeline_layout;
        info.renderPass = offscreen_pass;
        info.subpass = 0;
        info.basePipelineHandle = VK_NULL_HANDLE;
        info.basePipelineIndex = -1;

        vk_result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info,
                                              NULL, &offscreen_pipeline);
        if (vk_result)
            goto error;
    }

    ret = owl_renderer_begin_im_command_buffer(r);
    if (ret)
        goto error;

    {
        VkClearValue clear_value;
        VkRenderPassBeginInfo info;
        VkCommandBuffer const command_buffer = r->im_command_buffer;

        clear_value.color.float32[0] = 0.0F;
        clear_value.color.float32[1] = 0.0F;
        clear_value.color.float32[2] = 0.0F;
        clear_value.color.float32[3] = 1.0F;

        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.pNext = NULL;
        info.renderPass = offscreen_pass;
        info.framebuffer = offscreen_framebuffer;
        info.renderArea.offset.x = 0;
        info.renderArea.offset.y = 0;
        info.renderArea.extent.width = dimension;
        info.renderArea.extent.height = dimension;
        info.clearValueCount = 1;
        info.pClearValues = &clear_value;

        vkCmdBeginRenderPass(command_buffer, &info,
                             VK_SUBPASS_CONTENTS_INLINE);
    }

    {
        VkCommandBuffer const command_buffer = r->im_command_buffer;
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          offscreen_pipeline);
        vkCmdDraw(command_buffer, 3, 1, 0, 0);
    }

    {
        VkCommandBuffer const command_buffer = r->im_command_buffer;
        vkCmdEndRenderPass(command_buffer);
    }

    ret = owl_renderer_end_im_command_buffer(r);
    if (ret)
        goto error;

    ret = owl_renderer_begin_im_command_buffer(r);
    if (ret)
        goto error;

    {
        VkImageMemoryBarrier barrier;
        VkCommandBuffer const command_buffer = r->im_command_buffer;

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext = NULL;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = r->brdflut_map_image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(command_buffer,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0,
                             NULL, 1, &barrier);
    }

    ret = owl_renderer_end_im_command_buffer(r);
    if (ret)
        goto error;

    vkDestroyPipeline(device, offscreen_pipeline, NULL);
    vkDestroyPipelineLayout(device, offscreen_pipeline_layout, NULL);
    vkDestroyShaderModule(device, offscreen_fragment_shader, NULL);
    vkDestroyShaderModule(device, offscreen_vertex_shader, NULL);
    vkDestroyFramebuffer(device, offscreen_framebuffer, NULL);
    vkDestroyRenderPass(device, offscreen_pass, NULL);

    return OWL_OK;

error:
    if (offscreen_pipeline)
        vkDestroyPipeline(device, offscreen_pipeline, NULL);

    if (offscreen_pipeline_layout)
        vkDestroyPipelineLayout(device, offscreen_pipeline_layout, NULL);

    if (offscreen_fragment_shader)
        vkDestroyShaderModule(device, offscreen_fragment_shader, NULL);

    if (offscreen_vertex_shader)
        vkDestroyShaderModule(device, offscreen_vertex_shader, NULL);

    if (offscreen_framebuffer)
        vkDestroyFramebuffer(device, offscreen_framebuffer, NULL);

    if (offscreen_pass)
        vkDestroyRenderPass(device, offscreen_pass, NULL);

    if (r->brdflut_map_image_view)
        vkDestroyImageView(device, r->brdflut_map_image_view, NULL);

    if (r->brdflut_map_memory)
        vkFreeMemory(device, r->brdflut_map_memory, NULL);

    if (r->brdflut_map_image)
        vkDestroyImage(device, r->brdflut_map_image, NULL);

    return OWL_ERROR_FATAL;
}

static void owl_renderer_deinit_brdflut(struct owl_renderer *r)
{
    VkDevice const device = r->device;
    vkDestroyImageView(device, r->brdflut_map_image_view, NULL);
    vkFreeMemory(device, r->brdflut_map_memory, NULL);
    vkDestroyImage(device, r->brdflut_map_image, NULL);
}

OWLAPI int owl_renderer_init(struct owl_renderer *r, struct owl_plataform *p)
{
    uint32_t width;
    uint32_t height;
    owl_v3 up;
    int ret;
    float ratio;
    float const fov = OWL_DEGREES_AS_RADIANS(45.0F);
    float const near = 0.01;
    float const far = 512.0F;

    r->plataform = p;
    r->im_command_buffer = VK_NULL_HANDLE;
    r->skybox_loaded = 0;
    r->font_loaded = 0;
    r->num_frames = OWL_NUM_IN_FLIGHT_FRAMES;

    r->clear_values[0].color.float32[0] = 0.0F;
    r->clear_values[0].color.float32[1] = 0.0F;
    r->clear_values[0].color.float32[2] = 0.0F;
    r->clear_values[0].color.float32[3] = 1.0F;
    r->clear_values[1].depthStencil.depth = 1.0F;
    r->clear_values[1].depthStencil.stencil = 0.0F;

    owl_plataform_get_framebuffer_dimensions(p, &width, &height);
    r->width = width;
    r->height = height;

    ratio = (float)width / (float)height;

    OWL_V3_SET(r->camera_eye, 0.0F, -0.5F, 3.0F);
    OWL_V4_SET(r->camera_direction, 0.0F, 0.0F, 1.0F, 1.0F);
    OWL_V3_SET(up, 0.0F, 1.0F, 0.0F);

    owl_m4_perspective(fov, ratio, near, far, r->projection);
    owl_m4_look(r->camera_eye, r->camera_direction, up, r->view);

    ret = owl_renderer_init_instance(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize instance!\n");
        goto error;
    }

    ret = owl_renderer_init_surface(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize surface!\n");
        goto error_deinit_instance;
    }

    ret = owl_renderer_select_physical_device(r);
    if (ret) {
        OWL_DEBUG_LOG("Couldn't find physical device\n");
        goto error_deinit_surface;
    }

    ret = owl_renderer_clamp_dimensions(r);
    if (ret) {
        OWL_DEBUG_LOG("Couldn't clamp dimensions\n");
        goto error_deinit_surface;
    }

    ret = owl_renderer_init_device(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize device!\n");
        goto error_deinit_surface;
    }

    ret = owl_renderer_init_attachments(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize attachments\n");
        goto error_deinit_device;
    }

    ret = owl_renderer_init_render_passes(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize render passes!\n");
        goto error_deinit_attachments;
    }

    ret = owl_renderer_init_swapchain(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize swapchain!\n");
        goto error_deinit_render_passes;
    }

    ret = owl_renderer_init_pools(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize pools!\n");
        goto error_deinit_swapchain;
    }

    ret = owl_renderer_init_layouts(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize layouts!\n");
        goto error_deinit_pools;
    }

    ret = owl_renderer_init_graphics_pipelines(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize graphics pipelines!\n");
        goto error_deinit_layouts;
    }

    ret = owl_renderer_init_upload_buffer(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize upload heap!\n");
        goto error_deinit_graphics_pipelines;
    }

    ret = owl_renderer_init_samplers(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize samplers!\n");
        goto error_deinit_upload_buffer;
    }

    ret = owl_renderer_init_frames(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize frames!\n");
        goto error_deinit_samplers;
    }

    ret = owl_renderer_init_garbage(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to initialize garbage!\n");
        goto error_deinit_frames;
    }

    ret = owl_renderer_init_vertex_buffer(r, OWL_DEFAULT_BUFFER_SIZE);
    if (ret) {
        OWL_DEBUG_LOG("Filed to initilize vertex buffer!\n");
        goto error_deinit_garbage;
    }

    ret = owl_renderer_init_index_buffer(r, OWL_DEFAULT_BUFFER_SIZE);
    if (ret) {
        OWL_DEBUG_LOG("Filed to initilize index buffer!\n");
        goto error_deinit_vertex_buffer;
    }

    ret = owl_renderer_init_uniform_buffer(r, OWL_DEFAULT_BUFFER_SIZE);
    if (ret) {
        OWL_DEBUG_LOG("Filed to initilize uniform buffer!\n");
        goto error_deinit_index_buffer;
    }

    return OWL_OK;

error_deinit_index_buffer:
    owl_renderer_deinit_index_buffer(r);

error_deinit_vertex_buffer:
    owl_renderer_deinit_vertex_buffer(r);

error_deinit_garbage:
    owl_renderer_deinit_garbage(r);

error_deinit_frames:
    owl_renderer_deinit_frames(r);

error_deinit_samplers:
    owl_renderer_deinit_samplers(r);

error_deinit_upload_buffer:
    owl_renderer_deinit_upload_buffer(r);

error_deinit_graphics_pipelines:
    owl_renderer_deinit_graphics_pipelines(r);

error_deinit_layouts:
    owl_renderer_deinit_layouts(r);

error_deinit_pools:
    owl_renderer_deinit_pools(r);

error_deinit_swapchain:
    owl_renderer_deinit_swapchain(r);

error_deinit_render_passes:
    owl_renderer_deinit_render_passes(r);

error_deinit_attachments:
    owl_renderer_deinit_attachments(r);

error_deinit_device:
    owl_renderer_deinit_device(r);

error_deinit_surface:
    owl_renderer_deinit_surface(r);

error_deinit_instance:
    owl_renderer_deinit_instance(r);

error:
    return ret;
}

OWLAPI void owl_renderer_deinit(struct owl_renderer *r)
{
    vkDeviceWaitIdle(r->device);

    if (r->font_loaded)
        owl_renderer_unload_font(r);

    if (r->skybox_loaded)
        owl_renderer_unload_skybox(r);

    owl_renderer_deinit_uniform_buffer(r);
    owl_renderer_deinit_index_buffer(r);
    owl_renderer_deinit_vertex_buffer(r);
    owl_renderer_deinit_garbage(r);
    owl_renderer_deinit_frames(r);
    owl_renderer_deinit_samplers(r);
    owl_renderer_deinit_upload_buffer(r);
    owl_renderer_deinit_graphics_pipelines(r);
    owl_renderer_deinit_layouts(r);
    owl_renderer_deinit_pools(r);
    owl_renderer_deinit_swapchain(r);
    owl_renderer_deinit_render_passes(r);
    owl_renderer_deinit_attachments(r);
    owl_renderer_deinit_device(r);
    owl_renderer_deinit_surface(r);
    owl_renderer_deinit_instance(r);
}

OWLAPI int owl_renderer_update_dimensions(struct owl_renderer *r)
{
    uint32_t width;
    uint32_t height;
    float ratio;
    float const fov = OWL_DEGREES_AS_RADIANS(45.0F);
    float const near = 0.01;
    float const far = 10.0F;
    VkResult vk_result;
    int ret;

    vk_result = vkDeviceWaitIdle(r->device);
    if (vk_result) {
        ret = OWL_ERROR_FATAL;
        goto error;
    }

    owl_plataform_get_framebuffer_dimensions(r->plataform, &width, &height);
    r->width = width;
    r->height = height;

    ratio = (float)width / (float)height;
    owl_m4_perspective(fov, ratio, near, far, r->projection);

    owl_renderer_deinit_graphics_pipelines(r);
    owl_renderer_deinit_swapchain(r);
    owl_renderer_deinit_attachments(r);

    ret = owl_renderer_clamp_dimensions(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to clamp dimensions!\n");
        goto error;
    }

    ret = owl_renderer_init_attachments(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to resize attachments!\n");
        goto error;
    }

    ret = owl_renderer_init_swapchain(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to resize swapchain!\n");
        goto error_deinit_attachments;
    }

    ret = owl_renderer_init_graphics_pipelines(r);
    if (ret) {
        OWL_DEBUG_LOG("Failed to resize pipelines!\n");
        goto error_deinit_swapchain;
    }

    return OWL_OK;

error_deinit_swapchain:
    owl_renderer_deinit_swapchain(r);

error_deinit_attachments:
    owl_renderer_deinit_attachments(r);

error:
    return ret;
}

OWLAPI void *
owl_renderer_vertex_allocate(struct owl_renderer *r, uint64_t size,
                             struct owl_renderer_vertex_allocation *alloc)
{
    uint8_t *data = r->vertex_buffer_data;
    uint32_t const frame = r->frame;
    uint64_t const alignment = r->vertex_buffer_alignment;
    uint64_t required = size + r->vertex_buffer_offset;
    uint64_t aligned_size = r->vertex_buffer_aligned_size;

    if (r->vertex_buffer_size < required) {
        int ret;

        ret = owl_renderer_garbage_push_vertex(r);
        if (ret)
            return NULL;

        /* FIXME(samuel): ensure vertex buffer is valid after failure */
        ret = owl_renderer_init_vertex_buffer(r, required);
        if (ret)
            return NULL;

        data = r->vertex_buffer_data;
        required = size + r->vertex_buffer_offset;
        aligned_size = r->vertex_buffer_aligned_size;
    }

    alloc->offset = r->vertex_buffer_offset;
    alloc->buffer = r->vertex_buffers[frame];

    r->vertex_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

    return &data[frame * aligned_size + alloc->offset];
}

OWLAPI void owl_renderer_vertex_clear_offset(struct owl_renderer *r)
{
    r->vertex_buffer_last_offset = r->vertex_buffer_offset;
    r->vertex_buffer_offset = 0;
}

OWLAPI void *
owl_renderer_index_allocate(struct owl_renderer *r, uint64_t size,
                            struct owl_renderer_index_allocation *alloc)
{
    uint8_t *data = r->index_buffer_data;
    uint32_t const frame = r->frame;
    uint64_t const alignment = r->index_buffer_alignment;
    uint64_t required = size + r->index_buffer_offset;
    uint64_t aligned_size = r->index_buffer_aligned_size;

    if (r->index_buffer_size < required) {
        int ret;

        ret = owl_renderer_garbage_push_index(r);
        if (ret)
            return NULL;

        /* FIXME(samuel): ensure index buffer is valid after failure */
        ret = owl_renderer_init_index_buffer(r, required * 2);
        if (ret)
            return NULL;

        data = r->index_buffer_data;
        required = size + r->index_buffer_offset;
        aligned_size = r->index_buffer_aligned_size;
    }

    alloc->offset = r->index_buffer_offset;
    alloc->buffer = r->index_buffers[frame];

    r->index_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

    return &data[frame * aligned_size + alloc->offset];
}

OWLAPI void owl_renderer_index_clear_offset(struct owl_renderer *r)
{
    r->index_buffer_last_offset = r->index_buffer_offset;
    r->index_buffer_offset = 0;
}

OWLAPI void *
owl_renderer_uniform_allocate(struct owl_renderer *r, uint64_t size,
                              struct owl_renderer_uniform_allocation *alloc)
{
    uint8_t *data = r->uniform_buffer_data;
    uint32_t const frame = r->frame;
    uint64_t const alignment = r->uniform_buffer_alignment;
    uint64_t required = size + r->uniform_buffer_offset;
    uint64_t aligned_size = r->uniform_buffer_aligned_size;

    if (r->uniform_buffer_size < required) {
        int ret;

        ret = owl_renderer_garbage_push_uniform(r);
        if (ret)
            return NULL;

        /* FIXME(samuel): ensure uniform buffer is valid after failure */
        ret = owl_renderer_init_uniform_buffer(r, required * 2);
        if (ret)
            return NULL;

        data = r->uniform_buffer_data;
        required = size + r->uniform_buffer_offset;
        aligned_size = r->uniform_buffer_aligned_size;
    }

    alloc->offset = r->uniform_buffer_offset;
    alloc->buffer = r->uniform_buffers[frame];
    alloc->common_descriptor_set = r->uniform_pvm_descriptor_sets[frame];
    alloc->model_descriptor_set = r->uniform_model_descriptor_sets[frame];

    r->uniform_buffer_offset = OWL_ALIGN_UP_2(required, alignment);

    return &data[frame * aligned_size + alloc->offset];
}

OWLAPI void owl_renderer_uniform_clear_offset(struct owl_renderer *r)
{
    r->uniform_buffer_last_offset = r->uniform_buffer_offset;
    r->uniform_buffer_offset = 0;
}

#define OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result) \
    (VK_ERROR_OUT_OF_DATE_KHR == (vk_result) ||          \
     VK_SUBOPTIMAL_KHR == (vk_result) ||                 \
     VK_ERROR_SURFACE_LOST_KHR == (vk_result))

OWLAPI int owl_renderer_begin_frame(struct owl_renderer *r)
{
    VkResult vk_result = VK_SUCCESS;
    int ret = OWL_OK;

    uint64_t const timeout = (uint64_t)-1;
    uint32_t const num_clear_values = OWL_ARRAY_SIZE(r->clear_values);
    uint32_t const frame = r->frame;
    VkClearValue const *clear_values = r->clear_values;
    VkCommandBuffer command_buffer = r->submit_command_buffers[frame];
    VkSemaphore acquire_semaphore = r->acquire_semaphores[frame];
    VkDevice const device = r->device;

    vk_result = vkAcquireNextImageKHR(device, r->swapchain, timeout,
                                      acquire_semaphore, VK_NULL_HANDLE,
                                      &r->swapchain_image);
    if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result)) {
        ret = owl_renderer_update_dimensions(r);
        if (ret)
            return ret;

        vk_result = vkAcquireNextImageKHR(device, r->swapchain, timeout,
                                          acquire_semaphore, VK_NULL_HANDLE,
                                          &r->swapchain_image);
        if (vk_result)
            return OWL_ERROR_FATAL;
    }

    {
        VkFence in_flight_fence = r->in_flight_fences[frame];
        VkCommandPool command_pool = r->submit_command_pools[frame];

        vk_result =
                vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, timeout);
        if (vk_result)
            return OWL_ERROR_FATAL;

        vk_result = vkResetFences(device, 1, &in_flight_fence);
        if (vk_result)
            return OWL_ERROR_FATAL;

        vk_result = vkResetCommandPool(device, command_pool, 0);
        if (vk_result)
            return OWL_ERROR_FATAL;
    }

    owl_renderer_collect_garbage(r);

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

        framebuffer = r->swapchain_framebuffers[r->swapchain_image];
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.pNext = NULL;
        info.renderPass = r->main_render_pass;
        info.framebuffer = framebuffer;
        info.renderArea.offset.x = 0;
        info.renderArea.offset.y = 0;
        info.renderArea.extent.width = r->width;
        info.renderArea.extent.height = r->height;
        info.clearValueCount = num_clear_values;
        info.pClearValues = clear_values;

        vkCmdBeginRenderPass(command_buffer, &info,
                             VK_SUBPASS_CONTENTS_INLINE);
    }

    return OWL_OK;
}

OWLAPI int owl_renderer_end_frame(struct owl_renderer *r)
{
    uint32_t const frame = r->frame;
    VkCommandBuffer command_buffer = r->submit_command_buffers[frame];
    VkFence in_flight_fence = r->in_flight_fences[frame];
    VkSemaphore acquire_semaphore = r->acquire_semaphores[frame];
    VkSemaphore render_done_semaphore = r->render_done_semaphores[frame];

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

        vk_result =
                vkQueueSubmit(r->graphics_queue, 1, &info, in_flight_fence);
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
        info.pSwapchains = &r->swapchain;
        info.pImageIndices = &r->swapchain_image;
        info.pResults = NULL;

        vk_result = vkQueuePresentKHR(r->present_queue, &info);
        if (OWL_RENDERER_IS_SWAPCHAIN_OUT_OF_DATE(vk_result))
            return owl_renderer_update_dimensions(r);
    }

    r->frame = (r->frame + 1) % r->num_frames;
    owl_renderer_vertex_clear_offset(r);
    owl_renderer_index_clear_offset(r);
    owl_renderer_uniform_clear_offset(r);

    return OWL_OK;
}

OWLAPI void *
owl_renderer_upload_allocate(struct owl_renderer *r, uint64_t size,
                             struct owl_renderer_upload_allocation *alloc)
{
    VkResult vk_result;
    VkDevice const device = r->device;

    if (r->upload_buffer_in_use)
        goto error;

    r->upload_buffer_size = size;

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

        vk_result = vkCreateBuffer(device, &info, NULL, &r->upload_buffer);
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

        vkGetBufferMemoryRequirements(device, r->upload_buffer, &requirements);

        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.pNext = NULL;
        info.allocationSize = requirements.size;
        info.memoryTypeIndex = owl_renderer_find_memory_type(
                r, requirements.memoryTypeBits, properties);

        vk_result = vkAllocateMemory(device, &info, NULL,
                                     &r->upload_buffer_memory);
        if (vk_result)
            goto error_destroy_buffer;

        vk_result = vkBindBufferMemory(device, r->upload_buffer,
                                       r->upload_buffer_memory, 0);
        if (vk_result)
            goto error_free_memory;

        vk_result = vkMapMemory(device, r->upload_buffer_memory, 0,
                                r->upload_buffer_size, 0,
                                &r->upload_buffer_data);
        if (vk_result)
            goto error_free_memory;
    }

    alloc->buffer = r->upload_buffer;

    return r->upload_buffer_data;

error_free_memory:
    vkFreeMemory(device, r->upload_buffer_memory, NULL);

error_destroy_buffer:
    vkDestroyBuffer(device, r->upload_buffer, NULL);

error:
    return NULL;
}

OWLAPI void owl_renderer_upload_free(struct owl_renderer *r, void *data)
{
    VkDevice const device = r->device;

    OWL_UNUSED(data);
    OWL_ASSERT(r->upload_buffer_data == data);
    r->upload_buffer_in_use = 0;
    r->upload_buffer_size = 0;
    r->upload_buffer_data = NULL;

    vkFreeMemory(device, r->upload_buffer_memory, NULL);
    vkDestroyBuffer(device, r->upload_buffer, NULL);
}

OWLAPI int owl_renderer_load_font(struct owl_renderer *r, uint32_t size,
                                  char const *path)
{
    int ret = owl_font_init(r, path, size, &r->font);
    if (!ret)
        r->font_loaded = 1;
    return ret;
}

OWLAPI void owl_renderer_unload_font(struct owl_renderer *r)
{
    owl_font_deinit(r, &r->font);
    r->font_loaded = 0;
}

#define OWL_MAX_SKYBOX_PATH_LENGTH 256

OWLAPI int owl_renderer_load_skybox(struct owl_renderer *r, char const *path)
{
    int ret;
    struct owl_texture_desc texture_desc;
    VkDevice const device = r->device;

    texture_desc.type = OWL_TEXTURE_TYPE_CUBE;
    texture_desc.source = OWL_TEXTURE_SOURCE_FILE;
    texture_desc.path = path;
    texture_desc.pixels = NULL;
    texture_desc.width = 0;
    texture_desc.height = 0;
    texture_desc.format = OWL_RGBA8_SRGB;

    ret = owl_texture_init(r, &texture_desc, &r->skybox);
    if (ret)
        goto error;

    ret = owl_renderer_init_filter_maps(r);
    if (ret)
        goto error_deinit_texture;

    r->skybox_loaded = 1;

    ret = owl_renderer_init_brdflut(r);
    if (ret)
        goto error_deinit_filter_maps;

    {
        VkDescriptorSetAllocateInfo info;
        VkResult vk_result;

        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.pNext = NULL;
        info.descriptorPool = r->descriptor_pool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &r->model_environment_descriptor_set_layout;

        vk_result = vkAllocateDescriptorSets(device, &info,
                                             &r->environment_descriptor_set);
        if (vk_result) {
            ret = OWL_ERROR_FATAL;
            goto error_deinit_brdflut;
        }
    }

    {
        VkDescriptorImageInfo descriptors[3];
        VkWriteDescriptorSet writes[3];

        descriptors[0].sampler = VK_NULL_HANDLE;
        descriptors[0].imageView = r->irradiance_map_image_view;
        descriptors[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        descriptors[1].sampler = VK_NULL_HANDLE;
        descriptors[1].imageView = r->prefiltered_map_image_view;
        descriptors[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        descriptors[2].sampler = VK_NULL_HANDLE;
        descriptors[2].imageView = r->brdflut_map_image_view;
        descriptors[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].pNext = NULL;
        writes[0].dstSet = r->environment_descriptor_set;
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[0].pImageInfo = &descriptors[0];
        writes[0].pBufferInfo = NULL;
        writes[0].pTexelBufferView = NULL;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].pNext = NULL;
        writes[1].dstSet = r->environment_descriptor_set;
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[1].pImageInfo = &descriptors[1];
        writes[1].pBufferInfo = NULL;
        writes[1].pTexelBufferView = NULL;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].pNext = NULL;
        writes[2].dstSet = r->environment_descriptor_set;
        writes[2].dstBinding = 2;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writes[2].pImageInfo = &descriptors[2];
        writes[2].pBufferInfo = NULL;
        writes[2].pTexelBufferView = NULL;

        vkUpdateDescriptorSets(device, OWL_ARRAY_SIZE(writes), writes, 0,
                               NULL);
    }

    return OWL_OK;

error_deinit_brdflut:
    owl_renderer_deinit_brdflut(r);

error_deinit_filter_maps:
    owl_renderer_deinit_filter_maps(r);

error_deinit_texture:
    owl_texture_deinit(r, &r->skybox);

error:
    return ret;
}

OWLAPI void owl_renderer_unload_skybox(struct owl_renderer *r)
{
    VkDevice const device = r->device;

    vkFreeDescriptorSets(device, r->descriptor_pool, 1,
                         &r->environment_descriptor_set);
    owl_renderer_deinit_brdflut(r);
    owl_renderer_deinit_filter_maps(r);
    owl_texture_deinit(r, &r->skybox);
    r->skybox_loaded = 0;
}

OWLAPI uint32_t owl_renderer_find_memory_type(struct owl_renderer *r,
                                              uint32_t filter,
                                              uint32_t properties)
{
    uint32_t type;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDevice const physical_device = r->physical_device;

    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (type = 0; type < memory_properties.memoryTypeCount; ++type) {
        uint32_t flags = memory_properties.memoryTypes[type].propertyFlags;

        if ((flags & properties) && (filter & (1U << type)))
            return type;
    }

    return (uint32_t)-1;
}

OWLAPI int owl_renderer_begin_im_command_buffer(struct owl_renderer *r)
{
    VkResult vk_result = VK_SUCCESS;
    int ret = OWL_OK;
    VkDevice const device = r->device;

    OWL_ASSERT(!r->im_command_buffer);

    {
        VkCommandBufferAllocateInfo info;
        VkCommandBuffer *command_buffer = &r->im_command_buffer;

        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.pNext = NULL;
        info.commandPool = r->command_pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 1;

        vk_result = vkAllocateCommandBuffers(device, &info, command_buffer);
        if (VK_SUCCESS != vk_result) {
            ret = OWL_ERROR_FATAL;
            goto out;
        }
    }

    {
        VkCommandBufferBeginInfo info;
        VkCommandBuffer command_buffer = r->im_command_buffer;

        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.pNext = NULL;
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        info.pInheritanceInfo = NULL;

        vk_result = vkBeginCommandBuffer(command_buffer, &info);
        if (VK_SUCCESS != vk_result) {
            ret = OWL_ERROR_FATAL;
            goto error_im_command_buffer_deinit;
        }
    }

    goto out;

error_im_command_buffer_deinit:
    vkFreeCommandBuffers(device, r->command_pool, 1, &r->im_command_buffer);

out:
    return ret;
}

OWLAPI int owl_renderer_end_im_command_buffer(struct owl_renderer *r)
{
    VkSubmitInfo info;
    VkResult vk_result = VK_SUCCESS;
    int ret = OWL_OK;

    OWL_ASSERT(r->im_command_buffer);

    vk_result = vkEndCommandBuffer(r->im_command_buffer);
    if (VK_SUCCESS != vk_result) {
        ret = OWL_ERROR_FATAL;
        goto cleanup;
    }

    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.pNext = NULL;
    info.waitSemaphoreCount = 0;
    info.pWaitSemaphores = NULL;
    info.pWaitDstStageMask = NULL;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &r->im_command_buffer;
    info.signalSemaphoreCount = 0;
    info.pSignalSemaphores = NULL;

    vk_result = vkQueueSubmit(r->graphics_queue, 1, &info, NULL);
    if (VK_SUCCESS != vk_result) {
        ret = OWL_ERROR_FATAL;
        goto cleanup;
    }

    vk_result = vkQueueWaitIdle(r->graphics_queue);
    if (VK_SUCCESS != vk_result) {
        ret = OWL_ERROR_FATAL;
        goto cleanup;
    }

    r->im_command_buffer = VK_NULL_HANDLE;

cleanup:
    vkFreeCommandBuffers(r->device, r->command_pool, 1, &r->im_command_buffer);

    return ret;
}

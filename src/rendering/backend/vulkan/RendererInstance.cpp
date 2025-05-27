/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#include <rendering/backend/vulkan/RendererSwapchain.hpp>

#include <core/containers/Array.hpp>
#include <core/utilities/Span.hpp>
#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>
#include <system/AppContext.hpp>
#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

#include <system/vma/VmaUsage.hpp>

#include <Types.hpp>

#include <vector>
#include <optional>
#include <cstring>

#ifdef HYP_IOS
    #if MVK_IOS && MVK_OS_SIMULATOR
        #define MTLPixelFormatR8Unorm_sRGB MTLPixelFormatInvalid
        #define MTLPixelFormatRG8Unorm_sRGB MTLPixelFormatInvalid
        #define MTLPixelFormatB5G6R5Unorm MTLPixelFormatInvalid
        #define MTLPixelFormatA1BGR5Unorm MTLPixelFormatInvalid
        #define MTLPixelFormatABGR4Unorm MTLPixelFormatInvalid
    #endif
#endif

namespace hyperion {
namespace renderer {
namespace platform {

static VkPhysicalDevice PickPhysicalDevice(Span<VkPhysicalDevice> devices)
{
    if (!devices.Size())
    {
        return VK_NULL_HANDLE;
    }

    Features::DeviceRequirementsResult device_requirements_result(
        Features::DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR,
        "No device found");

    Features device_features;

    /* Check for a discrete/dedicated GPU with geometry shaders */
    for (VkPhysicalDevice device : devices)
    {
        device_features.SetPhysicalDevice(device);

        if (device_features.IsDiscreteGpu())
        {
            if ((device_requirements_result = device_features.SatisfiesMinimumRequirements()))
            {
                HYP_LOG(RenderingBackend, Info, "Select discrete device {}", device_features.GetDeviceName());

                return device;
            }
        }
    }

    /* No discrete gpu found, look for a device which satisfies requirements */
    for (VkPhysicalDevice device : devices)
    {
        device_features.SetPhysicalDevice(device);

        if ((device_requirements_result = device_features.SatisfiesMinimumRequirements()))
        {
            HYP_LOG(RenderingBackend, Info, "Select non-discrete device {}", device_features.GetDeviceName());

            return device;
        }
    }

    VkPhysicalDevice device = devices[0];
    device_features.SetPhysicalDevice(device);

    device_requirements_result = device_features.SatisfiesMinimumRequirements();

    HYP_LOG(RenderingBackend, Error, "No device found which satisfied the minimum requirements; selecting device {}.\nThe error message was: {}",
        device_features.GetDeviceName(), device_requirements_result.message);

    return device;
}

static Array<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
{
    uint32 device_count = 0;

    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

    AssertThrowMsg(device_count != 0, "No devices with Vulkan support found! "
                                      "Please update your graphics drivers or install a Vulkan compatible device.\n");

    Array<VkPhysicalDevice> devices;
    devices.Resize(device_count);

    vkEnumeratePhysicalDevices(instance, &device_count, devices.Data());

    return devices;
}

// Returns supported vulkan debug layers
static Array<const char*> CheckValidationLayerSupport(const Array<const char*>& requested_layers)
{
    Array<const char*> supported_layers;
    supported_layers.Reserve(requested_layers.Size());

    uint32 layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

    Array<VkLayerProperties> available_layers;
    available_layers.Resize(layers_count);

    vkEnumerateInstanceLayerProperties(&layers_count, available_layers.Data());

    for (const char* request : requested_layers)
    {
        bool layer_found = false;

        for (const auto& available_properties : available_layers)
        {
            if (!strcmp(available_properties.layerName, request))
            {
                layer_found = true;
                break;
            }
        }

        if (layer_found)
        {
            supported_layers.PushBack(request);
        }
        else
        {
            DebugLog(LogType::Warn, "Validation layer %s is unavailable!\n", request);
        }
    }

    return supported_layers;
}

ExtensionMap Instance<Platform::vulkan>::GetExtensionMap()
{
    return {
#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
        { VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false },
        { VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false },
        { VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, false },
        { VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, false },
#endif
        { VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, false },
        { VK_KHR_SPIRV_1_4_EXTENSION_NAME, false },
        { VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, false },
        { VK_KHR_SWAPCHAIN_EXTENSION_NAME, true },
        { VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME, false }
    };
}

void Instance<Platform::vulkan>::SetValidationLayers(Array<const char*> validation_layers)
{
    this->validation_layers = std::move(validation_layers);
}

#ifndef HYPERION_BUILD_RELEASE

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    LogType lt = LogType::Info;

    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        lt = LogType::RenDebug;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        lt = LogType::RenWarn;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        lt = LogType::RenError;
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        lt = LogType::RenInfo;
        break;
    default:
        break;
    }

    DebugLogRaw(lt, "Vulkan: [%s, %d]:\n\t%s\n",
        callback_data->pMessageIdName, callback_data->messageIdNumber, callback_data->pMessage);

    #if HYP_ENABLE_BREAKPOINTS
    if (lt == LogType::RenError)
    {
        HYP_BREAKPOINT;
    }
    #endif
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info, const VkAllocationCallbacks* callbacks, VkDebugUtilsMessengerEXT* debug_messenger)
{
    if (auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")))
    {
        return func(instance, create_info, callbacks, debug_messenger);
    }
    else
    {
        DebugLog(LogType::Error, "vkCreateDebugUtilsMessengerExt not present! disabling message callback...\n");

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

#endif

RendererResult Instance<Platform::vulkan>::SetupDebug()
{
    static const Array<const char*> layers {
        "VK_LAYER_KHRONOS_validation"
#if !defined(HYP_APPLE) || !HYP_APPLE
        ,
        "VK_LAYER_LUNARG_monitor"
#endif
    };

    Array<const char*> supported_layers = CheckValidationLayerSupport(layers);

    SetValidationLayers(std::move(supported_layers));

    HYPERION_RETURN_OK;
}

Instance<Platform::vulkan>::Instance()
    : m_surface(VK_NULL_HANDLE),
      m_instance(VK_NULL_HANDLE),
      m_swapchain(MakeRenderObject<VulkanSwapchain>())
{
}

RendererResult Instance<Platform::vulkan>::SetupDebugMessenger()
{
#ifndef HYPERION_BUILD_RELEASE
    VkDebugUtilsMessengerCreateInfoEXT messenger_info { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    messenger_info.messageSeverity = (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);
    messenger_info.messageType = (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    messenger_info.pfnUserCallback = &DebugCallback;
    messenger_info.pUserData = nullptr;

    HYPERION_VK_CHECK(CreateDebugUtilsMessengerEXT(m_instance, &messenger_info, nullptr, &this->debug_messenger));

    DebugLog(LogType::Info, "Using Vulkan Debug Messenger\n");
#endif
    HYPERION_RETURN_OK;
}

RendererResult Instance<Platform::vulkan>::Initialize(const AppContextBase& app_context, bool load_debug_layers)
{
    /* Set up our debug and validation layers */
    if (load_debug_layers)
    {
        HYPERION_BUBBLE_ERRORS(SetupDebug());
    }

    VkApplicationInfo app_info { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app_info.pApplicationName = app_context.GetAppName().Data();
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "HyperionEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    // Set target api version
    app_info.apiVersion = HYP_VULKAN_API_VERSION;

    VkInstanceCreateInfo create_info { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = uint32(validation_layers.Size());
    create_info.ppEnabledLayerNames = validation_layers.Data();
    create_info.flags = 0;

#if 0
    #if defined(HYP_APPLE) && HYP_APPLE
    // for vulkan sdk 1.3.216 and above, enumerate portability extension is required for
    // translation layers such as moltenvk.
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    #endif
#endif

    // Setup Vulkan extensions
    Array<const char*> extension_names;

    if (!app_context.GetVkExtensions(extension_names))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to load Vulkan extensions.");
    }

    extension_names.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

#if 0
    #if defined(HYP_APPLE) && HYP_APPLE && VK_HEADER_VERSION >= 216
    // add our enumeration extension to our instance extensions
    extension_names.PushBack(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    #endif
#endif

    DebugLog(LogType::Debug, "Got %llu extensions:\n", extension_names.Size());

    for (const char* extension_name : extension_names)
    {
        DebugLog(LogType::Debug, "\t%s\n", extension_name);
    }

    create_info.enabledExtensionCount = uint32(extension_names.Size());
    create_info.ppEnabledExtensionNames = extension_names.Data();

    DebugLog(LogType::Info, "Loading [%d] Instance extensions...\n", extension_names.Size());

    VkResult instance_result = vkCreateInstance(&create_info, nullptr, &m_instance);

    DebugLog(LogType::Info, "Instance result: %d\n", instance_result);
    HYPERION_VK_CHECK_MSG(
        instance_result,
        "Failed to create Vulkan Instance!");

    /* Create our renderable surface from SDL */
    AssertThrow(app_context.GetMainWindow() != nullptr);
    m_surface = app_context.GetMainWindow()->CreateVkSurface(this);

    /* Find and set up an adequate GPU for rendering and presentation */
    HYPERION_BUBBLE_ERRORS(CreateDevice());
    HYPERION_BUBBLE_ERRORS(CreateSwapchain());

    SetupDebugMessenger();
    m_device->SetupAllocator(this);

    HYPERION_RETURN_OK;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* callbacks)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr)
    {
        func(instance, debug_messenger, callbacks);
    }
    else
    {
        DebugLog(LogType::Error, "Extension for vkDestroyDebugUtilsMessengerEXT not supported!\n");
    }
}

RendererResult Instance<Platform::vulkan>::Destroy()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_device->Wait(), result);

    m_device->DestroyAllocator();

    SafeRelease(std::move(m_swapchain));

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    m_device->Destroy();
    delete m_device;
    m_device = nullptr;

#ifndef HYPERION_BUILD_RELEASE
    DestroyDebugUtilsMessengerEXT(m_instance, this->debug_messenger, nullptr);
#endif

    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;

    return result;
}

RendererResult Instance<Platform::vulkan>::CreateDevice(VkPhysicalDevice physical_device)
{
    /* If no physical device passed in, we select one */
    if (physical_device == VK_NULL_HANDLE)
    {
        Array<VkPhysicalDevice> devices = EnumeratePhysicalDevices(m_instance);
        physical_device = PickPhysicalDevice(Span<VkPhysicalDevice>(devices.Begin(), devices.End()));
    }

    m_device = new Device<Platform::vulkan>(physical_device, m_surface);
    m_device->SetRequiredExtensions(GetExtensionMap());

    const QueueFamilyIndices& family_indices = m_device->GetQueueFamilyIndices();

    /* Put into a set so we don't have any duplicate indices */
    const std::set<uint32> required_queue_family_indices {
        family_indices.graphics_family.Get(),
        family_indices.transfer_family.Get(),
        family_indices.present_family.Get(),
        family_indices.compute_family.Get()
    };

    /* Create a logical device to operate on */
    HYPERION_BUBBLE_ERRORS(m_device->Create(required_queue_family_indices));

    /* Get the internal queues from our device */

    HYPERION_RETURN_OK;
}

RendererResult Instance<Platform::vulkan>::CreateSwapchain()
{
    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Surface not created before initializing swapchain");
    }

    m_swapchain->m_surface = m_surface;
    HYPERION_BUBBLE_ERRORS(m_swapchain->Create());

    HYPERION_RETURN_OK;
}

RendererResult Instance<Platform::vulkan>::RecreateSwapchain()
{
    if (m_swapchain.IsValid())
    {
        // Cannot use SafeRelease here; will get NATIVE_WINDOW_IN_USE_KHR error
        m_swapchain->Destroy();
        m_swapchain.Reset();
    }

    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Surface not created before initializing swapchain");
    }

    m_swapchain = MakeRenderObject<VulkanSwapchain>();
    m_swapchain->m_surface = m_surface;
    HYPERION_BUBBLE_ERRORS(m_swapchain->Create());

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion

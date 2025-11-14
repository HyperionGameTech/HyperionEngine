/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/RenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/utilities/Span.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/debug/Debug.hpp>

#include <system/AppContext.hpp>

#include <system/vma/VmaUsage.hpp>

#include <engine/EngineGlobals.hpp>

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

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

static VkPhysicalDevice PickPhysicalDevice(Span<VkPhysicalDevice> devices)
{
    if (!devices.Size())
    {
        return VK_NULL_HANDLE;
    }

    VulkanFeatures::DeviceRequirementsResult deviceRequirementsResult(VulkanFeatures::DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "No device found");

    VulkanFeatures deviceFeatures;

    /* Check for a discrete/dedicated GPU with geometry shaders */
    for (VkPhysicalDevice device : devices)
    {
        deviceFeatures.SetPhysicalDevice(device);

        if (deviceFeatures.IsDiscreteGpu())
        {
            if ((deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements()))
            {
                HYP_LOG(RenderingBackend, Info, "Select discrete device {}", deviceFeatures.GetDeviceName());

                return device;
            }
        }
    }

    /* No discrete gpu found, look for a device which satisfies requirements */
    for (VkPhysicalDevice device : devices)
    {
        deviceFeatures.SetPhysicalDevice(device);

        if ((deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements()))
        {
            HYP_LOG(RenderingBackend, Info, "Select non-discrete device {}", deviceFeatures.GetDeviceName());

            return device;
        }
    }

    VkPhysicalDevice device = devices[0];
    deviceFeatures.SetPhysicalDevice(device);

    deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements();

    HYP_LOG(RenderingBackend, Error, "No device found which satisfied the minimum requirements; selecting device {}.\nThe error message was: {}",
        deviceFeatures.GetDeviceName(), deviceRequirementsResult.message);

    return device;
}

static Array<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
{
    uint32 deviceCount = 0;

    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    HYP_GFX_ASSERT(deviceCount != 0, "No devices with Vulkan support found! "
                                     "Please update your graphics drivers or install a Vulkan compatible device.\n");

    Array<VkPhysicalDevice> devices;
    devices.Resize(deviceCount);

    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.Data());

    return devices;
}

#ifdef HYP_DEBUG_MODE

// Returns supported vulkan debug layers
static Array<const char*> CheckValidationLayerSupport(const Array<const char*>& requestedLayers)
{
    Array<const char*> supportedLayers;
    supportedLayers.Reserve(requestedLayers.Size());

    uint32 layersCount;
    vkEnumerateInstanceLayerProperties(&layersCount, nullptr);

    Array<VkLayerProperties> availableLayers;
    availableLayers.Resize(layersCount);

    vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.Data());

    for (const char* request : requestedLayers)
    {
        bool layerFound = false;

        for (const auto& availableProperties : availableLayers)
        {
            if (Memory::StrCmp(availableProperties.layerName, request) == 0)
            {
                layerFound = true;

                break;
            }
        }

        if (layerFound)
        {
            supportedLayers.PushBack(request);
        }
        else
        {
            HYP_LOG(RenderingBackend, Warning, "Validation layer {} is unavailable", request);
        }
    }

    return supportedLayers;
}
#endif

ExtensionMap VulkanInstance::GetExtensionMap()
{
    ExtensionMap map;
    map[VK_KHR_SWAPCHAIN_EXTENSION_NAME] = true;
    map[VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME] = true;
    map[VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME] = true;

    map[VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME] = false;
    map[VK_KHR_SPIRV_1_4_EXTENSION_NAME] = false;

#ifdef HYP_DEBUG_MODE
    map[VK_EXT_DEBUG_UTILS_EXTENSION_NAME] = false;
#endif

#if HYP_FEATURES_ENABLE_RAYTRACING
    map[VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME] = false;
    map[VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME] = false;
    map[VK_KHR_RAY_QUERY_EXTENSION_NAME] = false;
    map[VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME] = false;
    map[VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME] = false;
#endif

#ifdef HYP_WINDOWS
    map[VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME] = false;
    map[VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME] = false;
#endif

    return map;
}

#ifdef HYP_DEBUG_MODE

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData)
{
    LogType lt = LogType::Info;

    switch (severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        HYP_LOG(RenderingBackend, Debug, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        HYP_LOG(RenderingBackend, Warning, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        HYP_LOG(RenderingBackend, Error, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);

        HYP_BREAKPOINT;

        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        HYP_LOG(RenderingBackend, Info, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    default:
        break;
    }

    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* callbacks, VkDebugUtilsMessengerEXT* debugMessenger)
{
    if (auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")))
    {
        return func(instance, createInfo, callbacks, debugMessenger);
    }
    else
    {
        HYP_LOG(RenderingBackend, Error, "vkCreateDebugUtilsMessengerExt not present! disabling message callback...\n");

        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* callbacks)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (func != nullptr)
    {
        func(instance, debugMessenger, callbacks);
    }
}

#endif

#ifdef HYP_DEBUG_MODE
RendererResult VulkanInstance::SetupDebug()
{
    static const Array<const char*> layers {
        "VK_LAYER_KHRONOS_validation"
#if !defined(HYP_APPLE) || !HYP_APPLE
        ,
        "VK_LAYER_LUNARG_monitor"
#endif
    };

    m_validationLayers = CheckValidationLayerSupport(layers);

    HYPERION_RETURN_OK;
}
#endif

VulkanInstance::VulkanInstance()
    : m_surface(VK_NULL_HANDLE),
      m_instance(VK_NULL_HANDLE),
      m_swapchain(CreateObject<VulkanSwapchain>())
{
}

VulkanInstance::~VulkanInstance()
{
    if (m_instance == VK_NULL_HANDLE)
    {
        return;
    }

    m_device->DestroyAllocator();

    SafeDelete(std::move(m_swapchain));

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

    m_device.Reset();

#ifdef HYP_DEBUG_MODE
    if (m_vkDebugMessenger != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessenger(m_instance, m_vkDebugMessenger, nullptr);
        m_vkDebugMessenger = VK_NULL_HANDLE;
    }
#endif

    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
}

#ifdef HYP_DEBUG_MODE
RendererResult VulkanInstance::SetupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT messengerInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    messengerInfo.messageSeverity = (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT);
    messengerInfo.messageType = (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    messengerInfo.pfnUserCallback = &DebugCallback;
    messengerInfo.pUserData = nullptr;

    VULKAN_CHECK(CreateDebugUtilsMessenger(m_instance, &messengerInfo, nullptr, &m_vkDebugMessenger));

    HYP_LOG(RenderingBackend, Info, "Enabling Vulkan debug messenger");

    HYPERION_RETURN_OK;
}
#endif

RendererResult VulkanInstance::Initialize(bool enableDebug)
{
#ifdef HYP_DEBUG_MODE
    /* Set up our debug and validation layers */
    if (enableDebug)
    {
        HYP_GFX_CHECK(SetupDebug());
    }
#endif

    HYP_GFX_ASSERT(g_appContext != nullptr, "AppContext must be set before initializing VulkanInstance");

    VkApplicationInfo appInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = g_appContext->GetAppName().Data();
    appInfo.applicationVersion = VK_MAKE_VERSION(HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
    appInfo.pEngineName = "HyperionEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(HYP_VERSION_MAJOR, HYP_VERSION_MINOR, HYP_VERSION_PATCH);
    // Set target api version
    appInfo.apiVersion = HYP_VULKAN_API_VERSION;

    VkInstanceCreateInfo createInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    createInfo.pApplicationInfo = &appInfo;
#ifdef HYP_DEBUG_MODE
    createInfo.enabledLayerCount = uint32(m_validationLayers.Size());
    createInfo.ppEnabledLayerNames = m_validationLayers.Data();
#endif
    createInfo.flags = 0;

#if HYP_MOLTENVK
    // for vulkan sdk 1.3.216 and above, enumerate portability extension is required for
    // translation layers such as moltenvk.
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Setup Vulkan extensions
    Array<const char*> extensionNames;

    if (RendererResult result = GetRenderBackend()->GetVkExtensions(extensionNames); result.HasError())
    {
        return result;
    }

#ifdef HYP_DEBUG_MODE
    if (enableDebug)
    {
        extensionNames.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    HYP_LOG(RenderingBackend, Info, "Found {} extensions:", extensionNames.Size());

    for (const char* extensionName : extensionNames)
    {
        HYP_LOG(RenderingBackend, Info, "\t{}", extensionName);
    }

    createInfo.enabledExtensionCount = uint32(extensionNames.Size());
    createInfo.ppEnabledExtensionNames = extensionNames.Data();

    HYP_LOG(RenderingBackend, Info, "Loading {} Instance extensions...", extensionNames.Size());

    VkResult instanceResult = vkCreateInstance(&createInfo, nullptr, &m_instance);
    VULKAN_CHECK_MSG(instanceResult, "Failed to create Vulkan Instance!");

    HYP_GFX_ASSERT(g_appContext->GetMainWindow() != nullptr);
    m_surface = GetRenderBackend()->CreateVkSurface(g_appContext->GetMainWindow(), this);

    /* Find and set up an adequate GPU for rendering and presentation */
    HYP_GFX_CHECK(CreateDevice());
    HYP_GFX_CHECK(CreateSwapchain());

#ifdef HYP_DEBUG_MODE
    if (enableDebug)
    {
        SetupDebugMessenger();
    }
#endif

    m_device->SetupAllocator(this);

    HYPERION_RETURN_OK;
}

RendererResult VulkanInstance::CreateDevice(VkPhysicalDevice physicalDevice)
{
    /* If no physical device passed in, we select one */
    if (physicalDevice == VK_NULL_HANDLE)
    {
        Array<VkPhysicalDevice> devices = EnumeratePhysicalDevices(m_instance);
        physicalDevice = PickPhysicalDevice(Span<VkPhysicalDevice>(devices.Begin(), devices.End()));
    }

    m_device = CreateObject<VulkanDevice>(physicalDevice, m_surface);
    m_device->SetWantedExtensions(GetExtensionMap());

    const QueueFamilyIndices& familyIndices = m_device->GetQueueFamilyIndices();

    /* Put into a set so we don't have any duplicate indices */
    Bitset queueFamilyIndices;
    queueFamilyIndices.Set(familyIndices.graphicsFamily.Get(), true);
    queueFamilyIndices.Set(familyIndices.transferFamily.Get(), true);
    queueFamilyIndices.Set(familyIndices.presentFamily.Get(), true);
    queueFamilyIndices.Set(familyIndices.computeFamily.Get(), true);

    /* Create a logical device to operate on */
    HYP_GFX_CHECK(m_device->Create(queueFamilyIndices.ToUInt64()));

    /* Get the internal queues from our device */

    HYPERION_RETURN_OK;
}

RendererResult VulkanInstance::CreateSwapchain()
{
    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Surface not created before initializing swapchain");
    }

    m_swapchain->m_surface = m_surface;
    HYP_GFX_CHECK(m_swapchain->Create());

    HYPERION_RETURN_OK;
}

RendererResult VulkanInstance::RecreateSwapchain()
{
    Handle<VulkanSwapchain> prevSwapchain = std::move(m_swapchain);

    if (m_surface == VK_NULL_HANDLE)
    {
        return HYP_MAKE_ERROR(RendererError, "Surface not created before initializing swapchain");
    }

    HYP_LOG(RenderingBackend, Info, "Recreating swapchain...");

    m_swapchain = CreateObject<VulkanSwapchain>();
    m_swapchain->m_surface = m_surface;
    m_swapchain->m_handle = prevSwapchain->m_handle;
    HYP_GFX_CHECK(m_swapchain->Create());

    SafeDelete(std::move(prevSwapchain));

    HYPERION_RETURN_OK;
}

} // namespace hyperion

/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>

#include <rendering/RenderInterface.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/utilities/Span.hpp>

#include <Core/debug/Debug.hpp>

#include <system/AppContext.hpp>

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

namespace Hyperion {

#ifdef HYP_DEBUG_MODE
constexpr bool EnableVulkanSynchronizationValidation = false;
constexpr bool EnableVulkanVerboseValidationLogging = false;
#endif

namespace CoreApi {
extern void UpdateGlobalConfig(const ConfigurationTable& mergeValues);
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

static VkPhysicalDevice PickPhysicalDevice(Span<VkPhysicalDevice> devices)
{
    if (!devices.Size())
    {
        return VK_NULL_HANDLE;
    }
    
    ConfigurationTable gpuConfig = CoreApi::GetGlobalConfig();

    const ConfigurationValue& cfgSelectedGpuIndex = gpuConfig.Get("System.SelectedGpu.Index");

    VulkanFeatures::DeviceRequirementsResult deviceRequirementsResult(VulkanFeatures::DeviceRequirementsResult::DEVICE_REQUIREMENTS_ERR, "No device found");
    VulkanFeatures deviceFeatures;

    Array<VkPhysicalDevice, VulkanTempAllocator> validDevices;
    validDevices.Reserve(devices.Size());
    
    for (VkPhysicalDevice device : devices)
    {
        deviceFeatures.SetPhysicalDevice(device);

        if (deviceFeatures.IsDiscreteGpu() || deviceFeatures.IsIntegratedGpu())
        {
            validDevices.PushBack(device);
        }
    }
    
    if (cfgSelectedGpuIndex.IsNumber() && cfgSelectedGpuIndex.AsNumber() < validDevices.Size())
    {
        for (uint32 deviceIndex = 0; deviceIndex < uint32(validDevices.Size()); deviceIndex++)
        {
            if (deviceIndex == cfgSelectedGpuIndex.ToUInt32())
            {
                deviceFeatures.SetPhysicalDevice(validDevices[deviceIndex]);

                if ((deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements()))
                {
                    HYP_LOG(RenderingBackend, Info, "Select {} device {}", deviceFeatures.IsDiscreteGpu() ? "discrete" : "integrated", deviceFeatures.GetDeviceName());

                    return validDevices[deviceIndex];
                }
            }
        }
    }

    uint32 deviceIndex = 0;

    // select dedicated GPU
    for (VkPhysicalDevice device : validDevices)
    {
        deviceFeatures.SetPhysicalDevice(device);

        if (!deviceFeatures.IsDiscreteGpu())
        {
            ++deviceIndex;

            continue;
        }

        if ((deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements()))
        {
            HYP_LOG(RenderingBackend, Info, "Select discrete device {}", deviceFeatures.GetDeviceName());
            
            gpuConfig.Set("System.SelectedGpu.Index", JSON::Number(deviceIndex));

            CoreApi::UpdateGlobalConfig(gpuConfig);

            return device;
        }
        
        ++deviceIndex;
    }

    deviceIndex = 0;

    // select integrated GPU
    for (VkPhysicalDevice device : validDevices)
    {
        deviceFeatures.SetPhysicalDevice(device);

        if (!deviceFeatures.IsIntegratedGpu())
        {
            ++deviceIndex;

            continue;
        }

        if ((deviceRequirementsResult = deviceFeatures.SatisfiesMinimumRequirements()))
        {
            HYP_LOG(RenderingBackend, Info, "Select integrated device {}", deviceFeatures.GetDeviceName());
            
            gpuConfig.Set("System.SelectedGpu.Index", JSON::Number(deviceIndex));

            CoreApi::UpdateGlobalConfig(gpuConfig);

            return device;
        }
        
        ++deviceIndex;
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

    Assert(deviceCount != 0, "No devices with Vulkan support found! "
                             "Please update your graphics drivers or install a Vulkan compatible device.\n");

    Array<VkPhysicalDevice> devices;
    devices.Resize(deviceCount);

    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.Data());

    return devices;
}

#ifdef HYP_DEBUG_MODE

// Returns supported vulkan debug layers
static Array<const char*, VulkanAllocator> CheckValidationLayerSupport(Span<const char*> requestLayers)
{
    Array<const char*, VulkanAllocator> supportedLayers;
    supportedLayers.Reserve(requestLayers.Size());

    uint32 layersCount;
    vkEnumerateInstanceLayerProperties(&layersCount, nullptr);

    Array<VkLayerProperties> availableLayers;
    availableLayers.Resize(layersCount);

    vkEnumerateInstanceLayerProperties(&layersCount, availableLayers.Data());

    for (const char* request : requestLayers)
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

#if HYP_FEATURES_ENABLE_RAY_TRACING
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

#ifdef HYP_EDITOR
    // enable external memory and other required extensions for interfacing with the editor application
#ifndef HYP_APPLE
    map[VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME] = true;
    map[VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME] = true;
#endif

#ifdef HYP_WINDOWS
    map[VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME] = true;
#endif

    /// \todo : other platforms
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
        HYP_LOG(RenderingBackend, Verbose, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        HYP_LOG(RenderingBackend, Warning, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        HYP_LOG(RenderingBackend, Error, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        HYP_LOG(RenderingBackend, Info, "Vulkan: [{}, {}]: {}",
            callbackData->pMessageIdName, callbackData->messageIdNumber, callbackData->pMessage);
        break;
    default:
        break;
    }

    if (String(callbackData->pMessageIdName).Contains("pDescriptorSets"))
    {
        // HYP_BREAKPOINT;
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
    static const char* s_requestLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    m_validationLayers = CheckValidationLayerSupport(s_requestLayers);

    return {};
}

#endif

VulkanInstance::VulkanInstance()
    : m_instance(VK_NULL_HANDLE),
      m_allocator(VK_NULL_HANDLE)
{
#ifdef HYP_DEBUG_MODE
    m_vkDebugMessenger = VK_NULL_HANDLE;
#endif
}

VulkanInstance::~VulkanInstance()
{
    if (m_instance == VK_NULL_HANDLE)
    {
        return;
    }

    m_device->DestroyAllocator();

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
    Assert(m_vkDebugMessenger == VK_NULL_HANDLE);

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        | (EnableVulkanVerboseValidationLogging ? VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT : 0);
    messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    messengerInfo.pfnUserCallback = &DebugCallback;
    messengerInfo.pUserData = nullptr;

    VULKAN_CHECK(CreateDebugUtilsMessenger(m_instance, &messengerInfo, nullptr, &m_vkDebugMessenger));

    HYP_LOG(RenderingBackend, Info, "Enabling Vulkan debug messenger");

    return {};
}
#endif

RendererResult VulkanInstance::Initialize(bool enableDebugLayers)
{
#ifdef HYP_DEBUG_MODE
    /* Set up our debug and validation layers */
    if (enableDebugLayers)
    {
        CheckResultOrReturn(SetupDebug());
    }
#endif

    Assert(g_appContext != nullptr, "AppContext must be set before initializing VulkanInstance");

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

    if (RendererResult result = g_renderInterface->GetVkExtensions(extensionNames); result.HasError())
    {
        return result;
    }

#ifdef HYP_DEBUG_MODE
    extensionNames.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Synchronization validation setup
    constexpr const char LayerName[] = "VK_LAYER_KHRONOS_validation";
    constexpr VkBool32 TrueValue = VK_TRUE;
    constexpr VkBool32 FalseValue = VK_TRUE;
    constexpr uint32 DuplicateMessageLimit = EnableVulkanVerboseValidationLogging ? 0 : 10;

    Array<VkLayerSettingEXT, VulkanTempAllocator> layerSettings;

    VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo { VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT };

    if (enableDebugLayers)
    {
        layerSettings.PushBack({ LayerName, "duplicate_message_limit", VK_LAYER_SETTING_TYPE_UINT32_EXT, 1, &DuplicateMessageLimit });

        layerSettings.PushBack({ LayerName, "validate_core", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &TrueValue });
        layerSettings.PushBack({ LayerName, "thread_safety", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &TrueValue });

        if (EnableVulkanSynchronizationValidation)
        {
            layerSettings.PushBack({ LayerName, "validate_sync", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &TrueValue });
        }

        if (EnableVulkanVerboseValidationLogging)
        {
            // No message limit for verbose logging.
            layerSettings.PushBack({ LayerName, "enable_message_limit", VK_LAYER_SETTING_TYPE_BOOL32_EXT, 1, &FalseValue });
        }

        if (!layerSettings.Empty())
        {
            layerSettingsCreateInfo.settingCount = uint32(layerSettings.Size());
            layerSettingsCreateInfo.pSettings = layerSettings.Data();

            VulkanHelpers::ChainNext(createInfo, &layerSettingsCreateInfo);
        }
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

    IDummyVulkanSurfaceContext* dummySurfaceContext = nullptr;
    VkSurfaceKHR surface = g_renderInterface->CreateSurface(nullptr, &dummySurfaceContext);

    Array<VkPhysicalDevice> devices = EnumeratePhysicalDevices(m_instance);
    VkPhysicalDevice physicalDevice = PickPhysicalDevice(Span<VkPhysicalDevice>(devices.Begin(), devices.End()));

    /* Find and set up an adequate GPU for rendering and presentation */
    CheckResultOrReturn(CreateDevice(physicalDevice, surface));

    delete dummySurfaceContext;
    vkDestroySurfaceKHR(m_instance, surface, nullptr);

#ifdef HYP_DEBUG_MODE
    if (enableDebugLayers)
    {
        SetupDebugMessenger();
    }
#endif

    m_device->SetupAllocator(this);

    return {};
}

RendererResult VulkanInstance::CreateDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    Assert(physicalDevice != VK_NULL_HANDLE && surface != VK_NULL_HANDLE);

    m_device = MakeHandle<VulkanDevice>(physicalDevice);
    m_device->SetWantedExtensions(GetExtensionMap());

    CheckResultOrReturn(m_device->Create(surface));

    return {};
}

} // namespace Hyperion

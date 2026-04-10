#include "RxDevice.hpp"

#include "Core/FxDefines.hpp"
#include "Core/FxSizedArray.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxPanic.hpp>

FX_SET_MODULE_NAME("Device")

///////////////////////////////
// Queue Families
///////////////////////////////

void RxQueueFamilies::FindGraphicsFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (int32 index = 0; index < RawFamilies.Size; index++) {
        VkQueueFamilyProperties& family_props = RawFamilies[index];

        // Presentation families will usually always have graphics support. Best to choose the most
        // capable here as our main family.
        if ((family_props.queueFlags & VK_QUEUE_GRAPHICS_BIT) && FamilyHasPresentSupport(device, surface, index)) {
            mGraphicsIndex = index;
            mPresentIndex = index;
        }
    }
}

void RxQueueFamilies::FindTransferFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    for (int32 index = 0; index < RawFamilies.Size; index++) {
        VkQueueFamilyProperties& family_props = RawFamilies[index];

        // We want a separate queue here for
        if ((family_props.queueFlags & VK_QUEUE_TRANSFER_BIT) && index != mGraphicsIndex) {
            mTransferIndex = index;
        }
    }
}

bool RxQueueFamilies::FamilyHasPresentSupport(VkPhysicalDevice device, VkSurfaceKHR surface, uint32 family_index)
{
    uint32 support = 0;

    VkResult status = vkGetPhysicalDeviceSurfaceSupportKHR(device, family_index, surface, &support);

    if (status != VK_SUCCESS) {
        FxLogError("Could not query present support for family at index {:d}", family_index);
        return false;
    }

    return support > 0;
}

void RxQueueFamilies::FindQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
{
    uint32 family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);

    RawFamilies.InitSize(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, RawFamilies.pData);

    FxLogInfo("Amount of queue families: {:d}", family_count);


    FindGraphicsFamily(physical_device, surface);
    FindTransferFamily(physical_device, surface);
    // uint32 index = 0;
    // for (const auto &family : RawFamilies) {
    //     if (mPresentIndex != QueueNull && mGraphicsIndex != QueueNull && mTransferIndex != QueueNull) {
    //         break;
    //     }

    //     if (family.queueCount == 0)
    //         continue;

    //     // check for a graphics family
    //     {
    //         if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
    //             mGraphicsIndex = index;
    //         }
    //     }

    //     // check for a present family
    //     {
    //         uint32 present_support = 0;

    //         VkResult status = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface,
    //         &present_support); if (status != VK_SUCCESS) {
    //             Log::Error("Could not retrieve physical device surface support", status);
    //             continue;
    //         }

    //         Log::Info("Present support: %d\n", present_support);
    //         if (present_support > 0) {
    //             mPresentIndex = index;
    //         }
    //     }

    //     if (mPresentIndex != QueueNull && mGraphicsIndex != QueueNull && mTransferIndex == QueueNull &&
    //     (family.queueFlags & VK_QUEUE_TRANSFER_BIT)) {
    //         mTransferIndex = index;
    //     }

    //     index++;
    // }
}


///////////////////////////////
// GPU Device
///////////////////////////////

bool RxGpuDevice::IsPhysicalDeviceSuitable(VkPhysicalDevice& physical)
{
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;

    RxQueueFamilies new_families;
    new_families.FindQueueFamilies(physical, mSurface);

    vkGetPhysicalDeviceFeatures(physical, &features);
    vkGetPhysicalDeviceProperties(physical, &properties);

    const uint32 version = properties.apiVersion;

    if (version > VK_MAKE_VERSION(1, 2, 0) && new_families.IsComplete()) {
        return true;
    }

    FxLogInfo("Device not suitable: (Vk: {}.{}.{}), Graphics?: {}, Present?: {}, Xfer?: {}, IsComplete?: {}",
              VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version),
              (new_families.GetGraphicsFamily() != RxQueueFamilies::scNullQueue),
              (new_families.GetPresentFamily() != RxQueueFamilies::scNullQueue),
              (new_families.GetTransferFamily() != RxQueueFamilies::scNullQueue), (new_families.IsComplete()));

    return false;
}

void RxGpuDevice::QueryQueues()
{
    vkGetDeviceQueue(Device, mQueueFamilies.GetGraphicsFamily(), 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, mQueueFamilies.GetPresentFamily(), 0, &PresentQueue);

    vkGetDeviceQueue(Device, mQueueFamilies.GetTransferFamily(), 0, &TransferQueue);
}

void RxGpuDevice::CreateLogicalDevice()
{
    if (Physical == nullptr) {
        PickPhysicalDevice();
    }

    if (!mQueueFamilies.IsComplete()) {
        mQueueFamilies.FindQueueFamilies(Physical, mSurface);
    }

    const auto queue_family_indices = mQueueFamilies.GetQueueIndexList();

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_family_indices.size());

    const float graphics_priorities[] = { 1.0f };
    float transfer_priorities[] = { 1.0f };

    queue_create_infos.push_back(VkDeviceQueueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mQueueFamilies.GetGraphicsFamily(),
        .queueCount = 1,
        .pQueuePriorities = graphics_priorities,
    });

    queue_create_infos.push_back(VkDeviceQueueCreateInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = mQueueFamilies.GetTransferFamily(),
        .queueCount = 1,
        .pQueuePriorities = transfer_priorities,
    });


    const VkPhysicalDeviceFeatures device_features {
        .fillModeNonSolid = true,
    };

    const char* device_extensions[] = {
#ifdef FX_PLATFORM_MACOS
        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
#endif
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    };

#ifdef FX_PLATFORM_MACOS

    // List of features that may not be available on all devices
    VkPhysicalDevicePortabilitySubsetFeaturesKHR portability_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
        .pNext = nullptr,
        .mutableComparisonSamplers = VK_TRUE, // For samplers that use compareOp's / SampleCmp in shaders
    };
#endif

    VkPhysicalDeviceVulkan11Features vk11_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
#ifdef FX_PLATFORM_MACOS
        .pNext = &portability_features,
#else
        .pNext = nullptr,
#endif
        .shaderDrawParameters = VK_TRUE,

    };


    const VkDeviceCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vk11_features,

        .queueCreateInfoCount = static_cast<uint32>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),

        // device specific layers (not used in modern vulkan)
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,

        // device specific extensions
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]),

        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &device_features,

    };

    const VkResult status = vkCreateDevice(Physical, &create_info, nullptr, &Device);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create logical device", status);
    }

    QueryQueues();
}

void RxGpuDevice::Create(VkInstance instance, VkSurfaceKHR surface)
{
    mInstance = instance;
    mSurface = surface;

    PickPhysicalDevice();

    mQueueFamilies.FindQueueFamilies(Physical, surface);

    CreateLogicalDevice();

    FxLogInfo("Device Queue Families: \n\tPresent: {:d}\n\tGraphics: {:d}\n\tTransfer: {:d}",
              mQueueFamilies.GetPresentFamily(), mQueueFamilies.GetGraphicsFamily(),
              mQueueFamilies.GetTransferFamily());
}

void RxGpuDevice::Destroy()
{
    vkDestroyDevice(Device, nullptr);

    Device = nullptr;
}

VkSurfaceFormatKHR RxGpuDevice::GetSurfaceFormat()
{
    uint32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Physical, mSurface, &format_count, nullptr);

    FxSizedArray<VkSurfaceFormatKHR> surface_formats;
    surface_formats.InitSize(format_count);

    vkGetPhysicalDeviceSurfaceFormatsKHR(Physical, mSurface, &format_count, surface_formats.pData);

    VkSurfaceFormatKHR best_format = surface_formats[0];

    for (VkSurfaceFormatKHR surface_format : surface_formats) {
        if (surface_format.format == VK_FORMAT_R16G16B16A16_SFLOAT) {
            return surface_format;
        }

        if (surface_format.format == VK_FORMAT_R8G8B8A8_UNORM) {
            best_format = surface_format;
        }
    }

    return best_format;
}

void RxGpuDevice::PickPhysicalDevice()
{
    // enumerate physical devices
    //
    uint32 device_count;
    VkTry(vkEnumeratePhysicalDevices(mInstance, &device_count, nullptr), "Could not enumerate physical devices");

    if (device_count == 0) {
        FxModulePanic("No usable physical devices found (no vulkan support!)", 0);
    }

    FxSizedArray<VkPhysicalDevice> physical_devices;
    physical_devices.InitSize(device_count);

    VkTry(vkEnumeratePhysicalDevices(mInstance, &device_count, physical_devices.pData),
          "Could not enumerate physical devices");

    for (VkPhysicalDevice& device : physical_devices) {
        if (IsPhysicalDeviceSuitable(device)) {
            Physical = device;
            break;
        }
    }
    if (Physical == nullptr) {
        FxModulePanic("Could not find a suitable physical device!", 0);
    }
}

void RxGpuDevice::WaitForIdle()
{
    if (!Device) {
        return;
    }

    vkDeviceWaitIdle(Device);
}

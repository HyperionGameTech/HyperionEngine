/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderDescriptorSet.hpp>
#include <rendering/AsyncCompute.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

namespace hyperion {

VulkanDevice::VulkanDevice(VkPhysicalDevice physical, VkSurfaceKHR surface)
    : m_device(VK_NULL_HANDLE),
      m_physical(physical),
      m_surface(surface),
      m_allocator(VK_NULL_HANDLE),
      m_features(MakeUnique<VulkanFeatures>())
{
    m_features->SetPhysicalDevice(m_physical);

    m_queueFamilyIndices = FindQueueFamilies(m_physical, m_surface);
}

VulkanDevice::~VulkanDevice()
{
}

void VulkanDevice::SetRenderSurface(const VkSurfaceKHR& surface)
{
    m_surface = surface;
}

void VulkanDevice::SetRequiredExtensions(const ExtensionMap& extensions)
{
    m_requiredExtensions = extensions;
}

VkDevice VulkanDevice::GetDevice()
{
    return m_device;
}

VkPhysicalDevice VulkanDevice::GetPhysicalDevice()
{
    return m_physical;
}

VkSurfaceKHR VulkanDevice::GetRenderSurface()
{
    HYP_GFX_ASSERT(m_surface != VK_NULL_HANDLE, "Surface has not been set!");

    return m_surface;
}

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices {};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    Array<VkQueueFamilyProperties> families;
    families.Resize(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.Data());

    constexpr auto possibleFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT;

    /* TODO: move over to QueueFamilyIndices */
    Array<uint32> foundIndices;

    const auto predicate = [&](uint32 index, VkQueueFlagBits expectedBits, bool expectDedicated) -> bool
    {
        const auto maskedBits = families[index].queueFlags & possibleFlags;

        /* When looking for a dedicate graphics queue, we'll make sure it supports presentation.
         * Some devices appear only to compute and are not graphical,
         * so we need to make sure it supports presenting to the user. */
        if (expectedBits == VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 supportsPresentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, surface, &supportsPresentation);

            if (!supportsPresentation)
            {
                return false;
            }
        }

        if (maskedBits & expectedBits)
        {
            if (!expectDedicated)
            {
                return true;
            }

            return std::find(foundIndices.begin(), foundIndices.end(), index) == foundIndices.end();
        }

        return false;
    };

    /* Find dedicated queues */
    for (uint32 i = 0; i < uint32(families.Size()) && !indices.IsComplete(); i++)
    {
        if (families[i].queueCount == 0)
        {
            HYP_LOG(RenderingBackend, Debug, "Queue family {} supports no queues, skipping", i);

            continue;
        }

        if (!indices.presentFamily.HasValue())
        {
            VkBool32 supportsPresentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresentation);

            if (supportsPresentation)
            {
                HYP_LOG(RenderingBackend, Debug, "Found presentation queue: {}", i);
                indices.presentFamily = i;
            }
        }

        if (!indices.graphicsFamily.HasValue())
        {
            if (predicate(i, VK_QUEUE_GRAPHICS_BIT, true))
            {
                HYP_LOG(RenderingBackend, Debug, "Found dedicated graphics presentation queue: {}", i);
                indices.graphicsFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }

        if (!indices.transferFamily.HasValue())
        {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, true))
            {
                HYP_LOG(RenderingBackend, Debug, "Found dedicated transfer queue: {}", i);
                indices.transferFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }

        if (!indices.computeFamily.HasValue())
        {
            if (predicate(i, VK_QUEUE_COMPUTE_BIT, true))
            {
                HYP_LOG(RenderingBackend, Debug, "Found dedicated compute queue: {}", i);
                indices.computeFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }
    }

    HYP_GFX_ASSERT(indices.presentFamily.HasValue(), "No present queue family found!");
    HYP_GFX_ASSERT(indices.graphicsFamily.HasValue(), "No graphics queue family found that supports presentation!");

    if (!indices.transferFamily.HasValue())
    {
        HYP_LOG(RenderingBackend, Warning, "No dedicated transfer queue family found!");
    }

    if (!indices.computeFamily.HasValue())
    {
        HYP_LOG(RenderingBackend, Warning, "No dedicated compute queue family found!");
    }

    /* Fallback -- find queue families (non-dedicated) */
    for (uint32 i = 0; i < families.Size() && !indices.IsComplete(); i++)
    {
        if (families[i].queueCount == 0)
        {
            HYP_LOG(RenderingBackend, Debug, "Queue family {} supports no queues, skipping", i);

            continue;
        }

        if (!indices.transferFamily.HasValue())
        {
            if (predicate(i, VK_QUEUE_TRANSFER_BIT, false))
            {
                HYP_LOG(RenderingBackend, Debug, "Found non-dedicated transfer queue {}", i);
                indices.transferFamily = i;
            }
        }

        if (!indices.computeFamily.HasValue())
        {
            if (predicate(i, VK_QUEUE_COMPUTE_BIT, false))
            {
                HYP_LOG(RenderingBackend, Debug, "Found non-dedicated compute queue {}", i);
                indices.computeFamily = i;
            }
        }
    }

    HYP_GFX_ASSERT(indices.IsComplete(), "Queue indices could not be created! Indices were:\n"
                                         "\tGraphics: %d\n"
                                         "\tTransfer: %d\n"
                                         "\tPresent: %d\n",
        "\tCompute: %d\n",
        indices.graphicsFamily.GetOr(0xBEEF),
        indices.transferFamily.GetOr(0xBEEF),
        indices.presentFamily.GetOr(0xBEEF),
        indices.computeFamily.GetOr(0xBEEF));

    return indices;
}

Array<VkExtensionProperties> VulkanDevice::GetSupportedExtensions()
{
    uint32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extensionCount, nullptr);

    Array<VkExtensionProperties> supportedExtensions;
    supportedExtensions.Resize(extensionCount);

    vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extensionCount, supportedExtensions.Data());

    return supportedExtensions;
}

ExtensionMap VulkanDevice::GetUnsupportedExtensions()
{
    const Array<VkExtensionProperties> extensionsSupported = GetSupportedExtensions();
    ExtensionMap unsupportedExtensions;

    for (const KeyValuePair<String, bool>& requiredExt : m_requiredExtensions)
    {
        auto supportedIt = extensionsSupported.FindIf(
            [&requiredExt](const auto& it)
            {
                return requiredExt.first == it.extensionName;
            });

        if (supportedIt == extensionsSupported.end())
        {
            unsupportedExtensions.Insert(requiredExt);
        }
    }

    return unsupportedExtensions;
}

RendererResult VulkanDevice::CheckDeviceSuitable(const ExtensionMap& unsupportedExtensions)
{
    if (unsupportedExtensions.Any())
    {
        HYP_LOG(RenderingBackend, Warning, "--- Unsupported Extensions ---\n");

        bool anyRequired = false;

        for (const auto& extension : unsupportedExtensions)
        {
            if (extension.second)
            {
                HYP_LOG(RenderingBackend, Error, "\t{} [REQUIRED]", extension.first);

                anyRequired = true;
            }
            else
            {
                HYP_LOG(RenderingBackend, Warning, "\t{}", extension.first);
            }
        }

        if (anyRequired)
        {
            return HYP_MAKE_ERROR(RendererError, "Device does not support required extensions");
        }
    }

    const VulkanSwapchainSupportDetails swapchainSupport = m_features->QuerySwapchainSupport(m_surface);
    const bool swapchainsAvailable = swapchainSupport.formats.Any() && swapchainSupport.presentModes.Any();

    if (!m_queueFamilyIndices.IsComplete())
    {
        return HYP_MAKE_ERROR(RendererError, "Device not supported -- indices setup was not complete.");
    }

    if (!swapchainsAvailable)
    {
        return HYP_MAKE_ERROR(RendererError, "Device not supported -- swapchains not available.");
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanDevice::SetupAllocator(VulkanInstance* instance)
{
    VmaVulkanFunctions vkfuncs {};
    vkfuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vkfuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo {};
    createInfo.vulkanApiVersion = HYP_VULKAN_API_VERSION;
    createInfo.physicalDevice = m_physical;
    createInfo.device = m_device;
    createInfo.instance = instance->GetInstance();
    createInfo.pVulkanFunctions = &vkfuncs;
    createInfo.flags = 0 | (m_features->IsRaytracingSupported() ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0);

    vmaCreateAllocator(&createInfo, &m_allocator);

    HYPERION_RETURN_OK;
}

void VulkanDevice::DebugLogAllocatorStats() const
{
    if (m_allocator != VK_NULL_HANDLE)
    {
        char* statsString;
        vmaBuildStatsString(m_allocator, &statsString, true);

        HYP_LOG(RenderingBackend, Info, "Pre-destruction VMA stats:\n%s\n", statsString);

        vmaFreeStatsString(m_allocator, statsString);
    }
}

RendererResult VulkanDevice::DestroyAllocator()
{
    if (m_allocator != VK_NULL_HANDLE)
    {
        DebugLogAllocatorStats();

        vmaDestroyAllocator(m_allocator);
        m_allocator = VK_NULL_HANDLE;
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanDevice::Wait() const
{
    RendererResult result = RendererResult {};

    if (m_queueGraphics.queue != VK_NULL_HANDLE)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueGraphics.queue), result);
    }

    if (m_queueTransfer.queue != VK_NULL_HANDLE)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueTransfer.queue), result);
    }

    if (m_queueCompute.queue != VK_NULL_HANDLE)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueCompute.queue), result);
    }

    if (m_queuePresent.queue != VK_NULL_HANDLE)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queuePresent.queue), result);
    }

    VULKAN_PASS_ERRORS(vkDeviceWaitIdle(m_device), result);

    return result;
}

RendererResult VulkanDevice::Create(uint32 requiredQueueFamilies)
{
    HYP_LOG(RenderingBackend, Debug, "Memory properties:\n");
    const auto& memoryProperties = m_features->GetPhysicalDeviceMemoryProperties();

    for (uint32 i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const auto& memoryType = memoryProperties.memoryTypes[i];
        const uint32 heapIndex = memoryType.heapIndex;

        HYP_LOG(RenderingBackend, Debug, "Memory type %lu:\t(index: %lu, flags: %lu)\n", i, heapIndex, memoryType.propertyFlags);

        const VkMemoryHeap& heap = memoryProperties.memoryHeaps[heapIndex];

        HYP_LOG(RenderingBackend, Debug, "\tHeap:\t\t(size: %llu, flags: %lu)\n", heap.size, heap.flags);
    }

    Array<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float priorities[] = { 1.0f };

    // for each queue family(for separate threads) we add them to
    // our device initialization data
    FOR_EACH_BIT(requiredQueueFamilies, familyIndex)
    {
        VkDeviceQueueCreateInfo queueInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.pQueuePriorities = priorities;
        queueInfo.queueCount = 1;
        queueInfo.queueFamilyIndex = uint32(familyIndex);

        queueCreateInfos.PushBack(queueInfo);
    }

    const ExtensionMap unsupportedExtensions = GetUnsupportedExtensions();
    const auto supportedExtensions = GetSupportedExtensions();

    HYP_GFX_CHECK(CheckDeviceSuitable(unsupportedExtensions));

    // no _required_ extensions were missing (otherwise would have caused an error)
    // so for each unsupported extension, remove it from out list of extensions
    for (auto& it : unsupportedExtensions)
    {
        HYP_GFX_ASSERT(!it.second, "Unsupported extension should not be 'required', should have failed earlier check");

        m_requiredExtensions.Erase(it.first);
    }

    Array<const char*> extensionNames;
    extensionNames.Reserve(m_requiredExtensions.Size());

    for (const auto& it : m_requiredExtensions)
    {
        extensionNames.PushBack(it.first.Data());
    }

    // Vulkan 1.3 requires VK_KHR_portability_subset to be enabled if it is found in vkEnumerateDeviceExtensionProperties()
    // https://vulkan.lunarg.com/doc/view/1.3.211.0/mac/1.3-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451
    {

        auto protabilitySubsetIt = supportedExtensions.FindIf(
            [](const auto& it)
            {
                return std::strcmp(it.extensionName, "VK_KHR_portability_subset") == 0;
            });

        if (protabilitySubsetIt != supportedExtensions.end())
        {
            extensionNames.PushBack("VK_KHR_portability_subset");
        }
    }

    HYP_LOG(RenderingBackend, Debug, "Required vulkan extensions:");
    HYP_LOG(RenderingBackend, Debug, "-----");

    for (const char* str : extensionNames)
    {
        HYP_LOG(RenderingBackend, Debug, "\t%s", str);
    }

    HYP_LOG(RenderingBackend, Debug, "-----");

    VkDeviceCreateInfo createInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.pQueueCreateInfos = queueCreateInfos.Data();
    createInfo.queueCreateInfoCount = uint32(queueCreateInfos.Size());
    // Setup Device extensions
    createInfo.enabledExtensionCount = uint32(extensionNames.Size());
    createInfo.ppEnabledExtensionNames = extensionNames.Data();
    // Setup Device Features
    // createInfo.pEnabledFeatures        = &features->GetPhysicalDeviceFeatures();
    createInfo.pNext = &m_features->GetPhysicalDeviceFeatures2();

    VULKAN_CHECK_MSG(
        vkCreateDevice(m_physical, &createInfo, nullptr, &m_device),
        "Could not create Device!");

    HYP_LOG(RenderingBackend, Debug, "Loading dynamic functions\n");
    m_features->SetDeviceFeatures(this);

    { // Create device queues
        m_queueGraphics = VulkanDeviceQueue {
            .type = VulkanDeviceQueueType::GRAPHICS,
            .queue = GetQueue(m_queueFamilyIndices.graphicsFamily.Get())
        };

        m_queueTransfer = VulkanDeviceQueue {
            .type = VulkanDeviceQueueType::TRANSFER,
            .queue = GetQueue(m_queueFamilyIndices.transferFamily.Get())
        };

        m_queuePresent = VulkanDeviceQueue {
            .type = VulkanDeviceQueueType::PRESENT,
            .queue = GetQueue(m_queueFamilyIndices.presentFamily.Get())
        };

        m_queueCompute = VulkanDeviceQueue {
            .type = VulkanDeviceQueueType::COMPUTE,
            .queue = GetQueue(m_queueFamilyIndices.computeFamily.Get())
        };

        VulkanDeviceQueue* queuesWithCommandBuffers[] = { &m_queueGraphics, &m_queueTransfer, &m_queueCompute };

        for (auto& it : queuesWithCommandBuffers)
        {
            for (uint32 commandBufferIndex = 0; commandBufferIndex < it->commandPools.Size(); commandBufferIndex++)
            {
                uint32 familyIndex = 0;

                switch (it->type)
                {
                case VulkanDeviceQueueType::GRAPHICS:
                    familyIndex = m_queueFamilyIndices.graphicsFamily.Get();
                    break;
                case VulkanDeviceQueueType::TRANSFER:
                    familyIndex = m_queueFamilyIndices.transferFamily.Get();
                    break;
                case VulkanDeviceQueueType::COMPUTE:
                    familyIndex = m_queueFamilyIndices.computeFamily.Get();
                    break;
                case VulkanDeviceQueueType::PRESENT:
                    familyIndex = m_queueFamilyIndices.presentFamily.Get();
                    break;
                default:
                    HYP_UNREACHABLE();
                }

                VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
                poolInfo.queueFamilyIndex = familyIndex;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

                VULKAN_CHECK_MSG(
                    vkCreateCommandPool(GetDevice(), &poolInfo, nullptr, &it->commandPools[commandBufferIndex]),
                    "Could not create Vulkan command pool");
            }
        }
    }

    HYPERION_RETURN_OK;
}

VkQueue VulkanDevice::GetQueue(uint32 queueFamilyIndex, uint32 queueIndex)
{
    HYP_GFX_ASSERT(m_device != VK_NULL_HANDLE);

    VkQueue queue;
    vkGetDeviceQueue(m_device, queueFamilyIndex, queueIndex, &queue);

    return queue;
}

void VulkanDevice::Destroy()
{
    VulkanDeviceQueue* queues[] = { &m_queueGraphics, &m_queueTransfer, &m_queueCompute, &m_queuePresent };

    for (VulkanDeviceQueue* queue : queues)
    {
        for (VkCommandPool commandPool : queue->commandPools)
        {
            vkDestroyCommandPool(m_device, commandPool, nullptr);
        }

        queue->commandPools = {};
    }

    if (m_device != VK_NULL_HANDLE)
    {
        /* By the time this destructor is called there should never
         * be a running queue, but just in case we will wait until
         * all the queues on our device are stopped. */
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
    }
}

} // namespace hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/AsyncCompute.hpp>

#include <Core/debug/Debug.hpp>

#include <VulkanDevice.generated.inl>

namespace Hyperion {

VulkanDevice::VulkanDevice(VkPhysicalDevice physical)
    : m_device(VK_NULL_HANDLE),
      m_physical(physical),
      m_allocator(VK_NULL_HANDLE),
      m_features(MakeUnique<VulkanFeatures>()),
      m_queueGraphics(nullptr),
      m_queueTransfer(nullptr),
      m_queuePresent(nullptr),
      m_queueCompute(nullptr)
{
    m_features->SetPhysicalDevice(m_physical);
}

VulkanDevice::~VulkanDevice()
{
    TSet<VulkanDeviceQueue*> queuesToDestroy;
    queuesToDestroy.Add(m_queueGraphics);
    queuesToDestroy.Add(m_queueTransfer);
    queuesToDestroy.Add(m_queuePresent);
    queuesToDestroy.Add(m_queueCompute);

    for (VulkanDeviceQueue* queue : queuesToDestroy)
    {
        if (!queue)
        {
            continue;
        }

        for (VkCommandPool commandPool : queue->commandPools)
        {
            vkDestroyCommandPool(m_device, commandPool, nullptr);
        }

        queue->commandPools = {};

        PoolDelete(*g_vulkanPool, queue);
    }

    m_queueGraphics = nullptr;
    m_queueTransfer = nullptr;
    m_queuePresent = nullptr;
    m_queueCompute = nullptr;

    if (m_device != VK_NULL_HANDLE)
    {
        /* By the time this destructor is called there should never
         * be a running queue, but just in case we will wait until
         * all the queues on our device are stopped. */
        vkDeviceWaitIdle(m_device);
        vkDestroyDevice(m_device, nullptr);
    }
}

void VulkanDevice::SetWantedExtensions(const ExtensionMap& extensions)
{
    m_wantedExtensions = extensions;
}

VkDevice VulkanDevice::GetDevice()
{
    return m_device;
}

VkPhysicalDevice VulkanDevice::GetPhysicalDevice()
{
    return m_physical;
}

QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
    const bool needPresentation = surface != VK_NULL_HANDLE;

    QueueFamilyIndices indices {};

    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    Array<VkQueueFamilyProperties> families;
    families.Resize(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, families.Data());

    constexpr auto possibleFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT;

    /* TODO: move over to QueueFamilyIndices */
    Array<uint32> foundIndices;

    const auto Predicate = [&](uint32 index, VkQueueFlagBits expectedBits, bool expectDedicated) -> bool
    {
        const uint32 maskedBits = families[index].queueFlags & possibleFlags;

        /* When looking for a dedicate graphics queue, we'll make sure it supports presentation.
         * Some devices appear only to compute and are not graphical,
         * so we need to make sure it supports presenting to the user. */
        if (needPresentation && expectedBits == VK_QUEUE_GRAPHICS_BIT)
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
            HYP_LOG(RenderingBackend, Verbose, "Queue family {} supports no queues, skipping", i);

            continue;
        }

        if (needPresentation && !indices.presentFamily.HasValue())
        {
            VkBool32 supportsPresentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresentation);

            if (supportsPresentation)
            {
                HYP_LOG(RenderingBackend, Verbose, "Found presentation queue: {}", i);
                indices.presentFamily = i;
            }
        }

        if (!indices.graphicsFamily.HasValue())
        {
            if (Predicate(i, VK_QUEUE_GRAPHICS_BIT, true))
            {
                HYP_LOG(RenderingBackend, Verbose, "Found dedicated graphics presentation queue: {}", i);
                indices.graphicsFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }

        if (!indices.transferFamily.HasValue())
        {
            if (Predicate(i, VK_QUEUE_TRANSFER_BIT, true))
            {
                HYP_LOG(RenderingBackend, Verbose, "Found dedicated transfer queue: {}", i);
                indices.transferFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }

        if (!indices.computeFamily.HasValue())
        {
            if (Predicate(i, VK_QUEUE_COMPUTE_BIT, true))
            {
                HYP_LOG(RenderingBackend, Verbose, "Found dedicated compute queue: {}", i);
                indices.computeFamily = i;
                foundIndices.PushBack(i);
                continue;
            }
        }
    }

    Assert(!needPresentation || indices.presentFamily.HasValue(), "No present queue family found!");
    Assert(indices.graphicsFamily.HasValue(), "No graphics queue family found that supports presentation!");

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
            HYP_LOG(RenderingBackend, Verbose, "Queue family {} supports no queues, skipping", i);

            continue;
        }

        if (!indices.transferFamily.HasValue())
        {
            if (Predicate(i, VK_QUEUE_TRANSFER_BIT, false))
            {
                HYP_LOG(RenderingBackend, Verbose, "Found non-dedicated transfer queue {}", i);
                indices.transferFamily = i;
            }
        }

        if (!indices.computeFamily.HasValue())
        {
            if (Predicate(i, VK_QUEUE_COMPUTE_BIT, false))
            {
                HYP_LOG(RenderingBackend, Verbose, "Found non-dedicated compute queue {}", i);
                indices.computeFamily = i;
            }
        }
    }

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

    for (auto it = m_wantedExtensions.Begin(); it != m_wantedExtensions.End();)
    {
        auto& ext = it->first;

        auto supportedIt = extensionsSupported.FindIf(
            [&ext](const auto& it)
            {
                return ext == it.extensionName;
            });

        if (supportedIt == extensionsSupported.end())
        {
            unsupportedExtensions.Insert(*it);

            if (!it->second) // keep req'd in map, remove optional
            {
                it = m_wantedExtensions.Erase(it);

                continue;
            }
        }

        ++it;
    }

    return unsupportedExtensions;
}

RendererResult VulkanDevice::CheckDeviceSuitable(const ExtensionMap& unsupportedExtensions)
{
    if (unsupportedExtensions.Any())
    {
        HYP_LOG(RenderingBackend, Warning, "--- Unsupported Extensions ---\n");

        Array<String> missingRequiredExtensionStrings;

        for (const auto& extension : unsupportedExtensions)
        {
            if (extension.second)
            {
                HYP_LOG(RenderingBackend, Error, "\t{} [REQUIRED]", extension.first);

                missingRequiredExtensionStrings.PushBack(extension.first);
            }
            else
            {
                HYP_LOG(RenderingBackend, Warning, "\t{}", extension.first);
            }
        }

        if (missingRequiredExtensionStrings.Any())
        {
            return HYP_MAKE_ERROR(
                RendererError,
                "Device does not support required extensions:\n\t{}",
                -1,
                String::Join(missingRequiredExtensionStrings, "\n\t"));
        }
    }

    if (!m_queueFamilyIndices.IsComplete())
    {
        return HYP_MAKE_ERROR(RendererError, "Device not supported -- indices setup was not complete.");
    }

    return {};
}

RendererResult VulkanDevice::SetupAllocator(VulkanInstance* instance)
{
    VmaVulkanFunctions vkFuncs {};
    vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo {};
    createInfo.vulkanApiVersion = HYP_VULKAN_API_VERSION;
    createInfo.physicalDevice = m_physical;
    createInfo.device = m_device;
    createInfo.instance = instance->GetInstance();
    createInfo.pVulkanFunctions = &vkFuncs;
    createInfo.flags = (m_features->IsRayTracingSupported() ? VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT : 0);

    VkResult result = vmaCreateAllocator(&createInfo, &m_allocator);
    VULKAN_CHECK(result);

    return {};
}

void VulkanDevice::DebugLogAllocatorStats() const
{
    if (m_allocator != VK_NULL_HANDLE)
    {
        char* statsString;
        vmaBuildStatsString(m_allocator, &statsString, true);

        HYP_LOG(RenderingBackend, Verbose, "Pre-destruction VMA stats:\n{}\n", statsString);

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

    return {};
}

RendererResult VulkanDevice::WaitIdle() const
{
    RendererResult result = RendererResult {};

    if (m_queueGraphics)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueGraphics->queue), result);
    }

    if (m_queueTransfer)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueTransfer->queue), result);
    }

    if (m_queueCompute)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queueCompute->queue), result);
    }

    if (m_queuePresent)
    {
        VULKAN_PASS_ERRORS(vkQueueWaitIdle(m_queuePresent->queue), result);
    }

    VULKAN_PASS_ERRORS(vkDeviceWaitIdle(m_device), result);

    return result;
}

RendererResult VulkanDevice::Create(VkSurfaceKHR surface)
{
    HYP_LOG(RenderingBackend, Verbose, "Memory properties:\n");
    const auto& memoryProperties = m_features->GetPhysicalDeviceMemoryProperties();

    for (uint32 i = 0; i < memoryProperties.memoryTypeCount; i++)
    {
        const auto& memoryType = memoryProperties.memoryTypes[i];
        const uint32 heapIndex = memoryType.heapIndex;

        HYP_LOG(RenderingBackend, Verbose, "Memory type {}:\t(index: {}, flags: {})\n", i, heapIndex, memoryType.propertyFlags);

        const VkMemoryHeap& heap = memoryProperties.memoryHeaps[heapIndex];

        HYP_LOG(RenderingBackend, Verbose, "\tHeap:\t\t(size: {}, flags: {})\n", heap.size, heap.flags);
    }

    Array<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float priorities[] = { 1.0f };

    InitQueueFamilies(surface);

    Bitset queueFamilyBits;
    queueFamilyBits.Set(m_queueFamilyIndices.graphicsFamily.Get(), true);
    queueFamilyBits.Set(m_queueFamilyIndices.transferFamily.Get(), true);
    queueFamilyBits.Set(m_queueFamilyIndices.presentFamily.Get(), true);
    queueFamilyBits.Set(m_queueFamilyIndices.computeFamily.Get(), true);

    FOR_EACH_BIT(queueFamilyBits.ToUInt64(), familyIndex)
    {
        VkDeviceQueueCreateInfo queueInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.pQueuePriorities = priorities;
        queueInfo.queueCount = 1;
        queueInfo.queueFamilyIndex = uint32(familyIndex);

        queueCreateInfos.PushBack(queueInfo);
    }

    const ExtensionMap unsupportedExtensions = GetUnsupportedExtensions();
    const auto supportedExtensions = GetSupportedExtensions();

    HYP_LOG(RenderingBackend, Info, "Vulkan device '{}' supports {} device extensions:", m_features->GetDeviceName(), supportedExtensions.Size());

    for (const VkExtensionProperties& extension : supportedExtensions)
    {
        HYP_LOG(RenderingBackend, Info, "\t{} (specVersion={})", extension.extensionName, extension.specVersion);
    }

    CheckResultOrReturn(CheckDeviceSuitable(unsupportedExtensions));

    for (auto& it : unsupportedExtensions)
    {
        m_wantedExtensions.Erase(it.first);
    }

    Array<const char*> extensionNames;
    extensionNames.Reserve(m_wantedExtensions.Size());

    for (const auto& it : m_wantedExtensions)
    {
        extensionNames.PushBack(it.first.Data());
    }

    HYP_LOG(RenderingBackend, Info, "Enabling {} Vulkan device extensions:", m_wantedExtensions.Size());

    for (const auto& it : m_wantedExtensions)
    {
        HYP_LOG(RenderingBackend, Info, "\t{}{}", it.first, it.second ? " [REQUIRED]" : "");
    }

    const auto HasExtensionSupport = [&](const char* name) -> bool
    {
        auto it = supportedExtensions.FindIf(
            [name](const auto& it)
            {
                return std::strcmp(it.extensionName, name) == 0;
            });

        return it != supportedExtensions.End();
    };

    // Vulkan 1.3 requires VK_KHR_portability_subset to be enabled if it is found in vkEnumerateDeviceExtensionProperties()
    // https://vulkan.lunarg.com/doc/view/1.3.211.0/mac/1.3-extensions/vkspec.html#VUID-VkDeviceCreateInfo-pProperties-04451
    {
        if (HasExtensionSupport("VK_KHR_portability_subset"))
        {
            extensionNames.PushBack("VK_KHR_portability_subset");
        }
    }

    HYP_LOG(RenderingBackend, Verbose, "Required vulkan extensions:");
    HYP_LOG(RenderingBackend, Verbose, "-----");

    for (const char* str : extensionNames)
    {
        HYP_LOG(RenderingBackend, Verbose, "- {}", str);
    }

    HYP_LOG(RenderingBackend, Verbose, "-----");

    VkDeviceCreateInfo createInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.pQueueCreateInfos = queueCreateInfos.Data();
    createInfo.queueCreateInfoCount = uint32(queueCreateInfos.Size());

#if defined(HYP_AFTERMATH) && HYP_AFTERMATH
    VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo { VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV };

    if (HasExtensionSupport(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME)
        && HasExtensionSupport(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME))
    {
        extensionNames.PushBack(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
        extensionNames.PushBack(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);

        /// https://docs.nvidia.com/nsight-aftermath/SDK/index.html
        VkDeviceDiagnosticsConfigFlagsNV aftermathFlags =
            VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV | // Enable automatic call stack checkpoints.
            VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |     // Enable tracking of resources.
            VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |     // Generate debug information for shaders.
            VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV; // Enable additional runtime shader error reporting.

        aftermathInfo.flags = aftermathFlags;

        VulkanHelpers::ChainNext(createInfo, &aftermathInfo);
    }
#endif

    // Setup Device extensions
    createInfo.enabledExtensionCount = uint32(extensionNames.Size());
    createInfo.ppEnabledExtensionNames = extensionNames.Data();
    // Setup Device Features
    // createInfo.pEnabledFeatures        = &features->GetPhysicalDeviceFeatures();

    VulkanHelpers::ChainNext(createInfo, const_cast<VkPhysicalDeviceFeatures2*>(&m_features->GetPhysicalDeviceFeatures2()));

    VULKAN_CHECK_MSG(
        vkCreateDevice(m_physical, &createInfo, nullptr, &m_device),
        "Could not create Device!");

    // Create command pools
    TSet<VulkanDeviceQueue*> deviceQueues;
    deviceQueues.Add(m_queueGraphics);
    deviceQueues.Add(m_queueTransfer);
    deviceQueues.Add(m_queueCompute);
    deviceQueues.Add(m_queuePresent);

    for (VulkanDeviceQueue* deviceQueue : deviceQueues)
    {
        if (!deviceQueue)
        {
            continue;
        }

        deviceQueue->queue = GetQueue(deviceQueue->familyIndex, 0);

        for (uint32 commandBufferIndex = 0; commandBufferIndex < deviceQueue->commandPools.Size(); commandBufferIndex++)
        {
            VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.queueFamilyIndex = deviceQueue->familyIndex;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            VkResult result = vkCreateCommandPool(GetDevice(), &poolInfo, nullptr, &deviceQueue->commandPools[commandBufferIndex]);
            Assert(result == VK_SUCCESS, "Could not create command pool for queue family index {}! VkResult: {}", deviceQueue->familyIndex, result);
        }
    }

    m_features->SetDeviceFeatures(this);

    return {};
}

VkQueue VulkanDevice::GetQueue(uint32 queueFamilyIndex, uint32 queueIndex)
{
    Assert(m_device != VK_NULL_HANDLE);

    VkQueue queue;
    vkGetDeviceQueue(m_device, queueFamilyIndex, queueIndex, &queue);

    return queue;
}

void VulkanDevice::InitQueueFamilies(VkSurfaceKHR surface)
{
    m_queueFamilyIndices = FindQueueFamilies(m_physical, surface);

    const bool needPresentation = surface != VK_NULL_HANDLE;

    Array<VulkanDeviceQueue, InlineAllocator<4>> queues;
    Array<VulkanDeviceQueue**, InlineAllocator<4>> queueMembers;

    queues.PushBack({
        .type = VulkanDeviceQueueType::GRAPHICS,
        .familyIndex = m_queueFamilyIndices.graphicsFamily.Get()
    });

    queueMembers.PushBack(&m_queueGraphics);

    queues.PushBack({
        .type = VulkanDeviceQueueType::TRANSFER,
        .familyIndex = m_queueFamilyIndices.transferFamily.Get()
    });

    queueMembers.PushBack(&m_queueTransfer);

    queues.PushBack({
        .type = VulkanDeviceQueueType::COMPUTE,
        .familyIndex = m_queueFamilyIndices.computeFamily.Get()
    });

    queueMembers.PushBack(&m_queueCompute);

    if (needPresentation)
    {
        queues.PushBack({
            .type = VulkanDeviceQueueType::PRESENT,
            .familyIndex = m_queueFamilyIndices.presentFamily.Get()
        });

        queueMembers.PushBack(&m_queuePresent);
    }

    TMap<uint32, VulkanDeviceQueue*> mapFamilyIndexToDeviceQueue;

    for (int i = 0; i < int(queues.Size()); i++)
    {
        VulkanDeviceQueue& deviceQueue = queues[i];
        VulkanDeviceQueue** ppDeviceQueue = queueMembers[i];

        auto insertResult = mapFamilyIndexToDeviceQueue.Insert(deviceQueue.familyIndex, &deviceQueue);

        if (insertResult.second)
        {
            // is unique; set member
            *ppDeviceQueue = HYP_POOL_NEW(g_vulkanPool, VulkanDeviceQueue, std::move(deviceQueue));
            AssertDebug(*ppDeviceQueue != nullptr);

            deviceQueue = **ppDeviceQueue; // swap out for the pooled one
            insertResult.first->second = *ppDeviceQueue;
        }
        else
        {
            AssertDebug(insertResult.first->second != nullptr);

            // reuse existing queue
            *ppDeviceQueue = insertResult.first->second;
        }
    }
}

} // namespace Hyperion

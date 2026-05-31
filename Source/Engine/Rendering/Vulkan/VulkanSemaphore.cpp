/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanSemaphore.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanResult.hpp>

#include <Rendering/Device.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <VulkanSemaphore.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderInterface RI;

VulkanSemaphore::VulkanSemaphore()
    : m_handle(VK_NULL_HANDLE),
      m_type(VulkanSemaphoreType::BINARY)
{
}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept
{
    if (this != &other)
    {
        if (m_handle != VK_NULL_HANDLE)
        {
            EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
                {
                    vkDestroySemaphore(RI.GetDevice()->GetDevice(), handle, nullptr);
                }));
        }

        m_handle = other.m_handle;
        m_type = other.m_type;
        other.m_handle = VK_NULL_HANDLE;
    }

    return *this;
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroySemaphore(RI.GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }
}

RendererResult VulkanSemaphore::Create()
{
    if (IsCreated())
    {
        return {};
    }

    VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkSemaphoreTypeCreateInfo timelineInfo { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };

    if (m_type == VulkanSemaphoreType::TIMELINE)
    {
        timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineInfo.initialValue = 0;

        semaphoreInfo.pNext = &timelineInfo;
    }

    VULKAN_CHECK_MSG(
        vkCreateSemaphore(RI.GetDevice()->GetDevice(), &semaphoreInfo, nullptr, &m_handle),
        "Failed to create semaphore");

#if HYP_DEBUG_MODE
    SetDebugName(debugName);
#endif

    return {};
}

#if HYP_DEBUG_MODE

void VulkanSemaphore::SetDebugName(Name name)
{
    if (!IsCreated())
    {
        return;
    }

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
        objectNameInfo.objectHandle = (uint64)m_handle;
        objectNameInfo.pObjectName = name.LookupString();

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

void VulkanSemaphore::Signal(uint64 value)
{
    Assert(IsTimeline() && IsCreated());

    VkSemaphoreSignalInfo signalInfo { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    signalInfo.semaphore = m_handle;
    signalInfo.value = value;

    VkResult result = RI.dynamicFunctions.vkSignalSemaphore(RI.GetDevice()->GetDevice(), &signalInfo);
    Assert(result == VK_SUCCESS, "Failed to signal timeline semaphore, VkResult: {}", result);
}

void VulkanSemaphore::WaitForValue(uint64 value, uint64 timeoutNs)
{
    Assert(IsTimeline() && IsCreated());

    VkSemaphoreWaitInfo waitInfo { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_handle;
    waitInfo.pValues = reinterpret_cast<uint64_t*>(&value);

    VkResult result = RI.dynamicFunctions.vkWaitSemaphores(RI.GetDevice()->GetDevice(), &waitInfo, timeoutNs);

    if (result == VK_TIMEOUT)
    {
        HYP_LOG(RenderingBackend, Warning, "Timeline semaphore wait timed out for value {}", value);
    }

    Assert(result == VK_SUCCESS || result == VK_TIMEOUT, "Failed to wait on timeline semaphore, VkResult: {}", result);
}

uint64 VulkanSemaphore::GetCounterValue() const
{
    Assert(IsTimeline() && IsCreated());

    uint64 value = 0;
    VkResult result = RI.dynamicFunctions.vkGetSemaphoreCounterValue(RI.GetDevice()->GetDevice(), m_handle, reinterpret_cast<uint64_t*>(&value));
    Assert(result == VK_SUCCESS, "Failed to get timeline semaphore counter value, VkResult: {}", result);

    return value;
}

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/Device.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <VulkanSemaphore.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern VulkanRenderInterface RI;

VulkanSemaphore::VulkanSemaphore()
    : m_handle(VK_NULL_HANDLE),
      m_type(VulkanSemaphoreType::BINARY)
{
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

    return {};
}

void VulkanSemaphore::Signal(uint64 value)
{
    Assert(IsTimeline() && IsCreated());

    VkSemaphoreSignalInfo signalInfo { VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    signalInfo.semaphore = m_handle;
    signalInfo.value = value;

    VkResult result = vkSignalSemaphore(RI.GetDevice()->GetDevice(), &signalInfo);
    Assert(result == VK_SUCCESS, "Failed to signal timeline semaphore, VkResult: {}", result);
}

void VulkanSemaphore::WaitForValue(uint64 value, uint64 timeoutNs)
{
    Assert(IsTimeline() && IsCreated());

    VkSemaphoreWaitInfo waitInfo { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_handle;
    waitInfo.pValues = reinterpret_cast<uint64_t*>(&value);

    VkResult result = vkWaitSemaphores(RI.GetDevice()->GetDevice(), &waitInfo, timeoutNs);

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
    VkResult result = vkGetSemaphoreCounterValue(RI.GetDevice()->GetDevice(), m_handle, reinterpret_cast<uint64_t*>(&value));
    Assert(result == VK_SUCCESS, "Failed to get timeline semaphore counter value, VkResult: {}", result);

    return value;
}

} // namespace Hyperion

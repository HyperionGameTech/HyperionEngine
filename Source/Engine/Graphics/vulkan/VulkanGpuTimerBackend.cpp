/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <VulkanPch.hpp>

#include "VulkanGpuTimerBackend.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandBuffer.hpp"
#include "VulkanFeatures.hpp"

#include <rendering/RenderInterface.hpp>

#include <Framework/EngineStats.hpp>

namespace Hyperion {

VulkanGpuTimerBackend::VulkanGpuTimerBackend()
    : GpuTimerBackendBase(),
      m_frames { }
{
}

VulkanGpuTimerBackend::~VulkanGpuTimerBackend()
{
    Shutdown();
}

bool VulkanGpuTimerBackend::Initialize(DeviceBase* device)
{
    VulkanDevice* vulkanDevice = static_cast<VulkanDevice*>(device);
    m_device = vulkanDevice;

    if (!m_device)
    {
        return false;
    }

    const VkPhysicalDeviceLimits& limits = m_device->GetFeatures().GetPhysicalDeviceProperties().limits;
    m_timestampPeriod = double(limits.timestampPeriod);

    const uint32 graphicsFamilyIndex = m_device->GetQueueFamilyIndices().graphicsFamily.Get();

    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_device->GetPhysicalDevice(), &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0 || graphicsFamilyIndex >= queueFamilyCount)
    {
        return false;
    }

    Array<VkQueueFamilyProperties> queueFamilyProperties;
    queueFamilyProperties.Resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_device->GetPhysicalDevice(), &queueFamilyCount, queueFamilyProperties.Data());

    if (queueFamilyProperties[graphicsFamilyIndex].timestampValidBits == 0)
    {
        return false;
    }

    VkQueryPoolCreateInfo queryPoolCreateInfo { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    queryPoolCreateInfo.pNext = nullptr;
    queryPoolCreateInfo.flags = 0;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = MaxGpuTimestampQueriesPerFrame;
    queryPoolCreateInfo.pipelineStatistics = 0;

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (vkCreateQueryPool(m_device->GetDevice(), &queryPoolCreateInfo, nullptr, &m_frames[i].queryPool) != VK_SUCCESS)
        {
            Shutdown();
            return false;
        }
    }

    m_isSupported = true;
    return true;
}

void VulkanGpuTimerBackend::Shutdown()
{
    if (!m_device)
    {
        return;
    }

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        if (m_frames[i].queryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(m_device->GetDevice(), m_frames[i].queryPool, nullptr);
            m_frames[i].queryPool = VK_NULL_HANDLE;
        }
        m_frames[i].resultsPending = false;
    }

    m_timers.Clear();

    m_isSupported = false;
}

bool VulkanGpuTimerBackend::IsSupported() const
{
    return m_isSupported;
}

double VulkanGpuTimerBackend::GetTimestampPeriod() const
{
    return m_timestampPeriod;
}

uint32 VulkanGpuTimerBackend::GetOrCreateQuerySlot(EngineStatGpuTimer* timer)
{
    if (timer->querySlotIndex < MaxGpuTimers)
    {
        return timer->querySlotIndex;
    }

    if (uint32(m_timers.Size()) >= MaxGpuTimers)
    {
        return UINT32_MAX;
    }

    m_timers.PushBack(timer);

    const uint32 slot = uint32(m_timers.Size() - 1);

    timer->querySlotIndex = slot;

    return slot;
}

void VulkanGpuTimerBackend::WriteStartTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (!m_isSupported || !cmd || !timer)
    {
        return;
    }

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    const uint32 slot = GetOrCreateQuerySlot(timer);

    if (slot >= MaxGpuTimers)
    {
        return;
    }

    PerFrameState& frameState = m_frames[frameIndex];

    if (!frameState.resultsPending)
    {
        vkCmdResetQueryPool(cmd->GetVulkanHandle(), frameState.queryPool, 0, MaxGpuTimestampQueriesPerFrame);
        frameState.resultsPending = true;
    }

    const uint32 queryIndex = slot * 2;

    vkCmdWriteTimestamp(cmd->GetVulkanHandle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frameState.queryPool, queryIndex);
}

void VulkanGpuTimerBackend::WriteStopTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (!m_isSupported || !cmd || !timer)
    {
        return;
    }

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    const uint32 slot = GetOrCreateQuerySlot(timer);

    if (slot >= MaxGpuTimers)
    {
        return;
    }

    PerFrameState& frameState = m_frames[frameIndex];

    if (!frameState.resultsPending)
    {
        vkCmdResetQueryPool(cmd->GetVulkanHandle(), frameState.queryPool, 0, MaxGpuTimestampQueriesPerFrame);
        frameState.resultsPending = true;
    }

    const uint32 queryIndex = slot * 2 + 1;

    vkCmdWriteTimestamp(cmd->GetVulkanHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frameState.queryPool, queryIndex);

    timer->querySlotIndex = UINT32_MAX;
}

double VulkanGpuTimerBackend::ComputeDeltaMs(uint64 start, uint64 end) const
{
    if (end <= start)
    {
        return 0.0;
    }

    return double(end - start) * m_timestampPeriod * 1e-6;
}

void VulkanGpuTimerBackend::ResolveFrameResults(uint32 completedFrameIndex)
{
    if (!m_isSupported)
    {
        return;
    }

    PerFrameState& frameState = m_frames[completedFrameIndex];

    if (!frameState.resultsPending)
    {
        return;
    }

    uint64 timestampsAndAvailability[MaxGpuTimestampQueriesPerFrame * 2] {};

    const VkResult result = vkGetQueryPoolResults(
        m_device->GetDevice(),
        frameState.queryPool,
        0,
        MaxGpuTimestampQueriesPerFrame,
        sizeof(timestampsAndAvailability),
        timestampsAndAvailability,
        sizeof(uint64) * 2,
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);

    if (result != VK_SUCCESS && result != VK_NOT_READY)
    {
        return;
    }

    frameState.resultsPending = false;

    for (uint32 i = 0; i < uint32(m_timers.Size()); i++)
    {
        EngineStatGpuTimer* timer = m_timers[i];

        if (!timer)
        {
            continue;
        }

        const uint32 startIndex = i * 2;
        const uint32 endIndex = i * 2 + 1;

        const bool startAvailable = timestampsAndAvailability[startIndex * 2 + 1] != 0;
        const bool endAvailable = timestampsAndAvailability[endIndex * 2 + 1] != 0;

        if (startAvailable && endAvailable)
        {
            const double elapsedMs = ComputeDeltaMs(timestampsAndAvailability[startIndex * 2], timestampsAndAvailability[endIndex * 2]);
            if (elapsedMs > 0.0)
            {
                timer->RecordElapsedMs(elapsedMs);
            }
        }
    }
}

} // namespace Hyperion

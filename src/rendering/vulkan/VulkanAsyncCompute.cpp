/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

VulkanAsyncCompute::VulkanAsyncCompute()
    : m_isSupported(false),
      m_isFallback(false)
{
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        m_commandBuffers[frameIndex] = MakeHandle<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        m_fences[frameIndex] = MakeHandle<VulkanFence>();
    }
}

VulkanAsyncCompute::~VulkanAsyncCompute()
{
    SafeDelete(std::move(m_commandBuffers));
    SafeDelete(std::move(m_fences));
}

RendererResult VulkanAsyncCompute::Create()
{
    HYP_SCOPE;

    Assert(g_renderInterface->GetDevice()->GetQueueFamilyIndices().IsComplete());

    VulkanDeviceQueue* queue = g_renderInterface->GetDevice()->GetComputeQueue();

    m_isSupported = g_renderInterface->GetDevice()->GetQueueFamilyIndices().computeFamily.HasValue();

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        queue = g_renderInterface->GetDevice()->GetGraphicsQueue();
    }

    for (const VulkanCommandBufferRef& commandBuffer : m_commandBuffers)
    {
        Assert(commandBuffer.IsValid());

        CheckResultOrReturn(commandBuffer->Create(queue->commandPools[0]));
    }

    for (const VulkanFenceRef& fence : m_fences)
    {
        CheckResultOrReturn(fence->Create());
    }

    return {};
}

RendererResult VulkanAsyncCompute::Submit(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    /// \todo : Call RenderQueue::Prepare to set descriptor sets to be used for the frame.

    CheckResultOrReturn(m_commandBuffers[frameIndex]->Begin());
    renderQueue.Execute(m_commandBuffers[frameIndex]);
    CheckResultOrReturn(m_commandBuffers[frameIndex]->End());

    VulkanDeviceQueue* computeQueue = g_renderInterface->GetDevice()->GetComputeQueue();

    return m_commandBuffers[frameIndex]->SubmitPrimary(computeQueue, m_fences[frameIndex], nullptr, nullptr);
}

RendererResult VulkanAsyncCompute::PrepareForFrame(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    CheckResultOrReturn(WaitForFence(frame));

    return {};
}

RendererResult VulkanAsyncCompute::WaitForFence(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    CheckResultOrReturn(m_fences[frameIndex]->Wait());
    CheckResultOrReturn(m_fences[frameIndex]->Reset());

    return {};
}

} // namespace Hyperion

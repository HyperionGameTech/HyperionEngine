/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanGpuBuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/util/SafeDeleter.hpp>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

VulkanAsyncCompute::VulkanAsyncCompute()
    : m_isSupported(false),
      m_isFallback(false)
{
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; ++frameIndex)
    {
        m_commandBuffers[frameIndex] = CreateObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        m_fences[frameIndex] = CreateObject<VulkanFence>();
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

    HYP_GFX_ASSERT(GetRenderBackend()->GetDevice()->GetQueueFamilyIndices().IsComplete());

    VulkanDeviceQueue* queue = GetRenderBackend()->GetDevice()->GetComputeQueue();

    m_isSupported = GetRenderBackend()->GetDevice()->GetQueueFamilyIndices().computeFamily.HasValue();

    if (!m_isSupported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        queue = GetRenderBackend()->GetDevice()->GetGraphicsQueue();
    }

    for (const VulkanCommandBufferRef& commandBuffer : m_commandBuffers)
    {
        HYP_GFX_ASSERT(commandBuffer.IsValid());

        HYP_GFX_CHECK(commandBuffer->Create(queue->commandPools[0]));
    }

    for (const VulkanFenceRef& fence : m_fences)
    {
        HYP_GFX_CHECK(fence->Create());
    }

    return {};
}

RendererResult VulkanAsyncCompute::Submit(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    // @TODO: Call RenderQueue::Prepare to set descriptor sets to be used for the frame.

    HYP_GFX_CHECK(m_commandBuffers[frameIndex]->Begin());
    renderQueue.Execute(m_commandBuffers[frameIndex]);
    HYP_GFX_CHECK(m_commandBuffers[frameIndex]->End());

    VulkanDeviceQueue* computeQueue = GetRenderBackend()->GetDevice()->GetComputeQueue();

    return m_commandBuffers[frameIndex]->SubmitPrimary(computeQueue, m_fences[frameIndex], nullptr, nullptr);
}

RendererResult VulkanAsyncCompute::PrepareForFrame(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    HYP_GFX_CHECK(WaitForFence(frame));

    return {};
}

RendererResult VulkanAsyncCompute::WaitForFence(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    RendererResult result = m_fences[frameIndex]->Wait();

    if (!result)
    {
        return result;
    }

    HYPERION_PASS_ERRORS(m_fences[frameIndex]->Reset(), result);

    return result;
}

} // namespace hyperion

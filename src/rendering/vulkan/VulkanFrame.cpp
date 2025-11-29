/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>

#include <rendering/RenderDevice.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <VulkanFrame.generated.inl>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

VulkanFrame::VulkanFrame()
    : FrameBase(0)
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex)
{
    FrameBase::m_frameIndex = frameIndex;
}

VulkanFrame::~VulkanFrame()
{
    SafeDelete(std::move(m_imageAvailableSemaphore));
    SafeDelete(std::move(m_renderFinishedSemaphore));
    SafeDelete(std::move(m_queueSubmitFence));
}

RendererResult VulkanFrame::Create()
{
    if (IsCreated())
    {
        return {};
    }

    m_imageAvailableSemaphore = CreateObject<VulkanSemaphore>();
    HYP_GFX_CHECK(m_imageAvailableSemaphore->Create());

    m_renderFinishedSemaphore = CreateObject<VulkanSemaphore>();
    HYP_GFX_CHECK(m_renderFinishedSemaphore->Create());

    m_queueSubmitFence = CreateObject<VulkanFence>();
    HYP_GFX_CHECK(m_queueSubmitFence->Create());

    return {};
}

RendererResult VulkanFrame::ResetFrameState()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_queueSubmitFence->Reset(), result);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    for (VulkanDescriptorSet* descriptorSet : m_usedDescriptorSets)
    {
        auto it = descriptorSet->GetCurrentFrames().FindAs(this);
        if (it != descriptorSet->GetCurrentFrames().End())
        {
            // Remove the current frame from the descriptor set's current frames
            // This is necessary to ensure that the descriptor set is not used in the next frame
            descriptorSet->GetCurrentFrames().Erase(it);
        }
    }
#endif

    m_usedDescriptorSets.Clear();

    if (OnFrameEnd.AnyBound())
    {
        OnFrameEnd(this);
        OnFrameEnd.RemoveAllDetached();
    }

    return result;
}

RendererResult VulkanFrame::Submit(VulkanDeviceQueue* deviceQueue, VulkanCommandBuffer* commandBuffer)
{
    AssertOnThread(g_renderThread);

    preRenderQueue.Prepare(this);
    renderQueue.Prepare(this);
    postRenderQueue.Prepare(this);

    UpdateUsedDescriptorSets();

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    commandBuffer->Begin();
    preRenderQueue.Execute(commandBuffer);
    renderQueue.Execute(commandBuffer);
    postRenderQueue.Execute(commandBuffer);
    commandBuffer->End();

    AssertDebug(m_imageAvailableSemaphore.IsValid()
        && m_renderFinishedSemaphore.IsValid()
        && m_queueSubmitFence.IsValid());

    return commandBuffer->SubmitPrimary(
        deviceQueue,
        m_queueSubmitFence,
        m_imageAvailableSemaphore,
        m_renderFinishedSemaphore);
}

RendererResult VulkanFrame::RecreateFence()
{
    if (m_queueSubmitFence.IsValid())
    {
        SafeDelete(std::move(m_queueSubmitFence));
    }

    m_queueSubmitFence = CreateObject<VulkanFence>();
    return m_queueSubmitFence->Create();
}

void VulkanFrame::ResetRenderPassStates()
{
#if 0
    for (VulkanRenderPass* renderPass : m_renderPasses)
    {
        for (VulkanAttachment* attachment : renderPass->GetAttachments())
        {
            AssertDebug(attachment != nullptr);

            if (!attachment)
            {
                continue;
            }

            const ResourceState currentState = attachment->GetImage()->GetResourceState();
        }
    }
#endif

    m_renderPasses.Clear();
}

} // namespace hyperion

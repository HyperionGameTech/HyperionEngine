/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/RenderDevice.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

VulkanFrame::VulkanFrame()
    : FrameBase(0),
      m_presentSemaphores({}, {})
{
}

VulkanFrame::VulkanFrame(uint32 frameIndex)
    : FrameBase(frameIndex),
      m_presentSemaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT })
{
    FrameBase::m_frameIndex = frameIndex;
}

VulkanFrame::~VulkanFrame()
{
    SafeDelete(std::move(m_queueSubmitFence));
}

RendererResult VulkanFrame::Create()
{
    HYP_GFX_CHECK(m_presentSemaphores.Create());

    m_queueSubmitFence = CreateObject<VulkanFence>();
    HYP_GFX_CHECK(m_queueSubmitFence->Create());

    HYPERION_RETURN_OK;
}

RendererResult VulkanFrame::ResetFrameState()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_queueSubmitFence->Reset(), result);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    for (DescriptorSetBase* descriptorSet : m_usedDescriptorSets)
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
    preRenderQueue.Prepare(this);
    renderQueue.Prepare(this);

    UpdateUsedDescriptorSets();

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    commandBuffer->Begin();
    preRenderQueue.Execute(commandBuffer);
    renderQueue.Execute(commandBuffer);
    commandBuffer->End();

    return commandBuffer->SubmitPrimary(deviceQueue, m_queueSubmitFence, &m_presentSemaphores);
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

} // namespace hyperion

/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

VulkanFrame::VulkanFrame()
    : FrameBase(0),
      m_present_semaphores({}, {})
{
}

VulkanFrame::VulkanFrame(uint32 frame_index)
    : FrameBase(frame_index),
      m_present_semaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT })
{
    FrameBase::m_frame_index = frame_index;
}

VulkanFrame::~VulkanFrame()
{
    AssertThrowMsg(!m_queue_submit_fence.IsValid(), "fc_queue_submit should have been released");
}

RendererResult VulkanFrame::Create()
{
    HYPERION_BUBBLE_ERRORS(m_present_semaphores.Create());

    m_queue_submit_fence = MakeRenderObject<VulkanFence>();
    HYPERION_BUBBLE_ERRORS(m_queue_submit_fence->Create());

    HYPERION_RETURN_OK;
}

RendererResult VulkanFrame::Destroy()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_present_semaphores.Destroy(), result);

    SafeRelease(std::move(m_queue_submit_fence));

    return result;
}

RendererResult VulkanFrame::ResetFrameState()
{
    RendererResult result;

    HYPERION_PASS_ERRORS(m_queue_submit_fence->Reset(), result);

#ifdef HYP_DESCRIPTOR_SET_TRACK_FRAME_USAGE
    for (DescriptorSetBase* descriptor_set : m_used_descriptor_sets)
    {
        auto it = descriptor_set->GetCurrentFrames().FindAs(this);
        if (it != descriptor_set->GetCurrentFrames().End())
        {
            // Remove the current frame from the descriptor set's current frames
            // This is necessary to ensure that the descriptor set is not used in the next frame
            descriptor_set->GetCurrentFrames().Erase(it);
        }
    }
#endif

    m_used_descriptor_sets.Clear();

    if (OnFrameEnd.AnyBound())
    {
        OnFrameEnd(this);
        OnFrameEnd.RemoveAllDetached();
    }

    return result;
}

RendererResult VulkanFrame::Submit(VulkanDeviceQueue* device_queue, const VulkanCommandBufferRef& command_buffer)
{
    m_command_list.Prepare(this);

    UpdateUsedDescriptorSets();

    if (OnPresent.AnyBound())
    {
        OnPresent(this);
        OnPresent.RemoveAllDetached();
    }

    command_buffer->Begin();
    m_command_list.Execute(command_buffer);
    command_buffer->End();

    return command_buffer->SubmitPrimary(device_queue, m_queue_submit_fence, &m_present_semaphores);
}

RendererResult VulkanFrame::RecreateFence()
{
    if (m_queue_submit_fence.IsValid())
    {
        SafeRelease(std::move(m_queue_submit_fence));
    }

    m_queue_submit_fence = MakeRenderObject<VulkanFence>();
    return m_queue_submit_fence->Create();
}

} // namespace hyperion
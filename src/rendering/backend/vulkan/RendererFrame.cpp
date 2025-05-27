/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

namespace renderer {

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

RendererResult VulkanFrame::RecreateFence()
{
    if (m_queue_submit_fence.IsValid())
    {
        SafeRelease(std::move(m_queue_submit_fence));
    }

    m_queue_submit_fence = MakeRenderObject<VulkanFence>();
    return m_queue_submit_fence->Create();
}

} // namespace renderer
} // namespace hyperion
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/AsyncCompute.hpp>
#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/RendererBuffer.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

VulkanAsyncCompute::VulkanAsyncCompute()
    : m_command_buffers({ MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY),
          MakeRenderObject<VulkanCommandBuffer>(CommandBufferType::COMMAND_BUFFER_PRIMARY) }),
      m_fences({ MakeRenderObject<VulkanFence>(),
          MakeRenderObject<VulkanFence>() }),
      m_is_supported(false),
      m_is_fallback(false)
{
}

VulkanAsyncCompute::~VulkanAsyncCompute()
{
    SafeRelease(std::move(m_command_buffers));
    SafeRelease(std::move(m_fences));
}

RendererResult VulkanAsyncCompute::Create()
{
    HYP_SCOPE;

    AssertThrow(GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices().IsComplete());

    VulkanDeviceQueue* queue = &GetRenderingAPI()->GetDevice()->GetComputeQueue();

    m_is_supported = GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices().compute_family.HasValue();

    if (!m_is_supported)
    {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");

        queue = &GetRenderingAPI()->GetDevice()->GetGraphicsQueue();
    }

    for (const VulkanCommandBufferRef& command_buffer : m_command_buffers)
    {
        AssertThrow(command_buffer.IsValid());

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(queue->command_pools[0]));
    }

    for (const VulkanFenceRef& fence : m_fences)
    {
        HYPERION_BUBBLE_ERRORS(fence->Create());
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanAsyncCompute::Submit(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    // @TODO: Call RHICommandList::Prepare to set descriptor sets to be used for the frame.

    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->Begin());
    m_command_list.Execute(m_command_buffers[frame_index]);
    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->End());

    VulkanDeviceQueue& compute_queue = GetRenderingAPI()->GetDevice()->GetComputeQueue();

    return m_command_buffers[frame_index]->SubmitPrimary(&compute_queue, m_fences[frame_index], nullptr);
}

RendererResult VulkanAsyncCompute::PrepareForFrame(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    HYPERION_BUBBLE_ERRORS(WaitForFence(frame));

    HYPERION_RETURN_OK;
}

RendererResult VulkanAsyncCompute::WaitForFence(VulkanFrame* frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    RendererResult result = m_fences[frame_index]->WaitForGPU();

    if (!result)
    {
        return result;
    }

    HYPERION_PASS_ERRORS(m_fences[frame_index]->Reset(), result);

    return result;
}

} // namespace hyperion
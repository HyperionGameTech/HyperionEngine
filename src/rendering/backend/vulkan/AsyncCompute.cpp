/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/AsyncCompute.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {
namespace platform {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

template <>
RendererResult AsyncCompute<Platform::VULKAN>::WaitForFence(Frame<Platform::VULKAN> *frame);

template <>
AsyncCompute<Platform::VULKAN>::AsyncCompute()
    : m_command_buffers({
          MakeRenderObject<CommandBuffer<Platform::VULKAN>>(CommandBufferType::COMMAND_BUFFER_PRIMARY),
          MakeRenderObject<CommandBuffer<Platform::VULKAN>>(CommandBufferType::COMMAND_BUFFER_PRIMARY)
      }),
      m_fences({
          MakeRenderObject<Fence<Platform::VULKAN>>(),
          MakeRenderObject<Fence<Platform::VULKAN>>()
      }),
      m_is_supported(false),
      m_is_fallback(false)
{
}

template <>
AsyncCompute<Platform::VULKAN>::~AsyncCompute()
{
    SafeRelease(std::move(m_command_buffers));
    SafeRelease(std::move(m_fences));
}

template <>
RendererResult AsyncCompute<Platform::VULKAN>::Create()
{
    HYP_SCOPE;

    AssertThrow(GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices().IsComplete());

    DeviceQueue<Platform::VULKAN> *queue = &GetRenderingAPI()->GetDevice()->GetComputeQueue();

    m_is_supported = GetRenderingAPI()->GetDevice()->GetQueueFamilyIndices().compute_family.HasValue();

    if (!m_is_supported) {
        HYP_LOG(RenderingBackend, Warning, "Dedicated compute queue not supported, using graphics queue for compute operations");
        
        queue = &GetRenderingAPI()->GetDevice()->GetGraphicsQueue();
    }
    
    for (CommandBufferRef<Platform::VULKAN> &command_buffer : m_command_buffers) {
        AssertThrow(command_buffer.IsValid());

        command_buffer->GetPlatformImpl().command_pool = queue->command_pools[0];

        HYPERION_BUBBLE_ERRORS(command_buffer->Create());
    }
    
    for (FenceRef<Platform::VULKAN> &fence : m_fences) {
        HYPERION_BUBBLE_ERRORS(fence->Create());
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult AsyncCompute<Platform::VULKAN>::Submit(Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    RHICommandList &command_list = m_command_lists[frame_index];

    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->Begin());
    command_list.Execute(m_command_buffers[frame_index]);
    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->End());

    DeviceQueue<Platform::VULKAN> &compute_queue = GetRenderingAPI()->GetDevice()->GetComputeQueue();

    return m_command_buffers[frame_index]->SubmitPrimary(&compute_queue, m_fences[frame_index], nullptr);
}

template <>
RendererResult AsyncCompute<Platform::VULKAN>::PrepareForFrame(Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();
    
    HYPERION_BUBBLE_ERRORS(WaitForFence(frame));

    HYPERION_RETURN_OK;
}

template <>
RendererResult AsyncCompute<Platform::VULKAN>::WaitForFence(Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    RendererResult result = m_fences[frame_index]->WaitForGPU();

    if (!result) {
        return result;
    }

    HYPERION_PASS_ERRORS(m_fences[frame_index]->Reset(), result);

    return result;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
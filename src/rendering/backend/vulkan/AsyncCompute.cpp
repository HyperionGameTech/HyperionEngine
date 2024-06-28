/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/AsyncCompute.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <util/profiling/ProfileScope.hpp>

namespace hyperion::renderer {

namespace platform {

template <>
Result AsyncCompute<Platform::VULKAN>::WaitForFence(Device<Platform::VULKAN> *device, Frame<Platform::VULKAN> *frame);

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
Result AsyncCompute<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    HYP_SCOPE;

    AssertThrow(device->GetQueueFamilyIndices().IsComplete());

    DeviceQueue<Platform::VULKAN> *queue = &device->GetComputeQueue();

    m_is_supported = device->GetQueueFamilyIndices().compute_family.HasValue();

    if (!m_is_supported) {
        HYP_LOG(RenderingBackend, LogLevel::WARNING, "Dedicated compute queue not supported, using graphics queue for compute operations");
        
        queue = &device->GetGraphicsQueue();
    }
    
    for (CommandBufferRef<Platform::VULKAN> &command_buffer : m_command_buffers) {
        AssertThrow(command_buffer.IsValid());

        command_buffer->GetPlatformImpl().command_pool = queue->command_pools[0];

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device));
    }
    
    for (FenceRef<Platform::VULKAN> &fence : m_fences) {
        HYPERION_BUBBLE_ERRORS(fence->Create(device));
    }

    HYPERION_RETURN_OK;
}

template <>
Result AsyncCompute<Platform::VULKAN>::Submit(Device<Platform::VULKAN> *device, Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();

    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->End(device));

    DeviceQueue<Platform::VULKAN> &compute_queue = device->GetComputeQueue();

    return m_command_buffers[frame_index]->SubmitPrimary(&compute_queue, m_fences[frame_index], nullptr);
}

template <>
Result AsyncCompute<Platform::VULKAN>::PrepareForFrame(Device<Platform::VULKAN> *device, Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();
    
    HYPERION_BUBBLE_ERRORS(WaitForFence(device, frame));

    HYPERION_BUBBLE_ERRORS(m_command_buffers[frame_index]->Begin(device));

    HYPERION_RETURN_OK;
}

template <>
Result AsyncCompute<Platform::VULKAN>::WaitForFence(Device<Platform::VULKAN> *device, Frame<Platform::VULKAN> *frame)
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();

    Result result = m_fences[frame_index]->WaitForGPU(device);

    if (!result) {
        return result;
    }

    HYPERION_PASS_ERRORS(m_fences[frame_index]->Reset(device), result);

    return result;
}

template <>
void AsyncCompute<Platform::VULKAN>::InsertBarrier(
    Frame<Platform::VULKAN> *frame,
    const GPUBufferRef<Platform::VULKAN> &buffer,
    ResourceState resource_state
) const
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();

    buffer->InsertBarrier(m_command_buffers[frame_index], resource_state, ShaderModuleType::COMPUTE);
}

template <>
void AsyncCompute<Platform::VULKAN>::Dispatch(
    Frame<Platform::VULKAN> *frame,
    const ComputePipelineRef<Platform::VULKAN> &ref,
    Extent3D extent
) const
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();

    ref->Bind(m_command_buffers[frame_index]);
    ref->Dispatch(m_command_buffers[frame_index], extent);
}

template <>
void AsyncCompute<Platform::VULKAN>::Dispatch(
    Frame<Platform::VULKAN> *frame,
    const ComputePipelineRef<Platform::VULKAN> &ref,
    Extent3D extent,
    const DescriptorTableRef<Platform::VULKAN> &descriptor_table,
    const ArrayMap<Name, ArrayMap<Name, uint>> &offsets
) const
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();

    descriptor_table->Bind<ComputePipelineRef<Platform::VULKAN>>(
        m_command_buffers[frame_index],
        frame_index,
        ref,
        offsets
    );

    ref->Bind(m_command_buffers[frame_index]);
    ref->Dispatch(m_command_buffers[frame_index], extent);
}

} // namespace platform

} // namespace hyperion::renderer
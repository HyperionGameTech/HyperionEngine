/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererQueue.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <core/math/MathUtil.hpp>
#include <Constants.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <>
FrameHandler<Platform::VULKAN>::FrameHandler(uint32 num_frames, NextImageFunction next_image)
    : m_next_image(next_image),
      m_acquired_image_index(0),
      m_current_frame_index(0)
{
}

template <>
RendererResult FrameHandler<Platform::VULKAN>::CreateFrames(
    Device<Platform::VULKAN> *device,
    DeviceQueue<Platform::VULKAN> *queue
)
{
    for (uint32 i = 0; i < m_frames.Size(); i++) {
        VkCommandPool pool = queue->command_pools[0];
        AssertThrow(pool != VK_NULL_HANDLE);
        
        CommandBufferRef<Platform::VULKAN> command_buffer = MakeRenderObject<CommandBuffer<Platform::VULKAN>>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
        command_buffer->GetPlatformImpl().command_pool = pool;
        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device));

        FrameRef<Platform::VULKAN> frame = MakeRenderObject<Frame<Platform::VULKAN>>(i);

        HYPERION_BUBBLE_ERRORS(frame->Create(
            device,
            std::move(command_buffer)
        ));

        m_frames[i] = std::move(frame);
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult FrameHandler<Platform::VULKAN>::PrepareFrame(
    Device<Platform::VULKAN> *device,
    Swapchain<Platform::VULKAN> *swapchain,
    bool &out_needs_recreate
)
{
    static const auto HandleFrameResult = [](VkResult result, bool &out_needs_recreate) -> RendererResult
    {
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
            out_needs_recreate = true;

            HYPERION_RETURN_OK;
        }

        HYPERION_VK_CHECK(result);

        HYPERION_RETURN_OK;
    };

    const FrameRef<Platform::VULKAN> &frame = GetCurrentFrame();

    HYPERION_BUBBLE_ERRORS(frame->GetFence()->WaitForGPU(device, true));

    HYPERION_BUBBLE_ERRORS(HandleFrameResult(frame->GetFence()->GetPlatformImpl().last_frame_result, out_needs_recreate));

    HYPERION_BUBBLE_ERRORS(frame->GetFence()->Reset(device));

    HYPERION_BUBBLE_ERRORS(m_next_image(device, swapchain, frame, &m_acquired_image_index, out_needs_recreate));

    HYPERION_RETURN_OK;
}

template <>
void FrameHandler<Platform::VULKAN>::NextFrame()
{
    m_current_frame_index = (m_current_frame_index + 1) % max_frames_in_flight;
}

template <>
RendererResult FrameHandler<Platform::VULKAN>::PresentFrame(
    DeviceQueue<Platform::VULKAN> *queue,
    Swapchain<Platform::VULKAN> *swapchain
) const
{
    const FrameRef<Platform::VULKAN> &frame = GetCurrentFrame();

    const auto &signal_semaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR present_info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = uint32(signal_semaphores.size());
    present_info.pWaitSemaphores = signal_semaphores.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->GetPlatformImpl().handle;
    present_info.pImageIndices = &m_acquired_image_index;
    present_info.pResults = nullptr;

    HYPERION_VK_CHECK(vkQueuePresentKHR(queue->queue, &present_info));

    HYPERION_RETURN_OK;
}

template <>
RendererResult FrameHandler<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_frames));

    return RendererResult { };
}

} // namespace platform
} // namespace renderer
} // namespace hyperion

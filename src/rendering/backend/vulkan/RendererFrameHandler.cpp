#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererQueue.hpp>

#include <math/MathUtil.hpp>
#include <Constants.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <>
FrameHandler<Platform::VULKAN>::FrameHandler(UInt num_frames, NextImageFunction next_image)
    : m_per_frame_data(MathUtil::Min(num_frames, max_frames_in_flight)),
      m_next_image(next_image),
      m_acquired_image_index(0),
      m_current_frame_index(0)
{
}

template <>
FrameHandler<Platform::VULKAN>::~FrameHandler() = default;

template <>
Result FrameHandler<Platform::VULKAN>::CreateFrames(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_per_frame_data.NumFrames() >= 1);
    
    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        auto frame = std::make_unique<Frame<Platform::VULKAN>>(i);

        HYPERION_BUBBLE_ERRORS(frame->Create(
            device,
            m_per_frame_data[i].Get<CommandBuffer<Platform::VULKAN>>()
        ));

        m_per_frame_data[i].Set<Frame<Platform::VULKAN>>(std::move(frame));
    }

    DebugLog(LogType::Debug, "Allocated [%d] frames\n", m_per_frame_data.NumFrames());

    HYPERION_RETURN_OK;
}

template <>
Result FrameHandler<Platform::VULKAN>::PrepareFrame(Device<Platform::VULKAN> *device, Swapchain *swapchain)
{
    auto *frame = GetCurrentFrameData().Get<Frame<Platform::VULKAN>>();

    VkResult fence_result;

    frame->fc_queue_submit->WaitForGPU(device, true, &fence_result);

    if (fence_result == VK_SUBOPTIMAL_KHR || fence_result == VK_ERROR_OUT_OF_DATE_KHR) {
        DebugLog(LogType::Debug, "Waiting -- image result was %d\n", fence_result);

        vkDeviceWaitIdle(device->GetDevice());

        /* TODO: regenerate framebuffers and swapchain */
    }

    HYPERION_VK_CHECK(fence_result);
    HYPERION_BUBBLE_ERRORS(frame->fc_queue_submit->Reset(device));

    HYPERION_BUBBLE_ERRORS(m_next_image(device, swapchain, frame, &m_acquired_image_index));

    HYPERION_RETURN_OK;
}

template <>
void FrameHandler<Platform::VULKAN>::NextFrame()
{
    m_current_frame_index = (m_current_frame_index + 1) % NumFrames();
}

template <>
Result FrameHandler<Platform::VULKAN>::PresentFrame(DeviceQueue *queue, Swapchain *swapchain) const
{
    auto *frame = GetCurrentFrameData().Get<Frame<Platform::VULKAN>>();

    const auto &signal_semaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};

    present_info.waitSemaphoreCount = UInt32(signal_semaphores.size());
    present_info.pWaitSemaphores = signal_semaphores.data();

    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->swapchain;
    present_info.pImageIndices = &m_acquired_image_index;
    present_info.pResults = nullptr;

    HYPERION_VK_CHECK(vkQueuePresentKHR(queue->queue, &present_info));

    HYPERION_RETURN_OK;
}

template <>
Result FrameHandler<Platform::VULKAN>::CreateCommandBuffers(Device<Platform::VULKAN> *device, DeviceQueue *queue)
{
    auto result = Result::OK;

    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        auto command_buffer = std::make_unique<CommandBuffer<Platform::VULKAN>>(
            CommandBufferType::COMMAND_BUFFER_PRIMARY
        );

        VkCommandPool pool = queue->command_pools[0];
        AssertThrow(pool != VK_NULL_HANDLE);

        HYPERION_PASS_ERRORS(command_buffer->Create(device, pool), result);

        m_per_frame_data[i].Set<CommandBuffer<Platform::VULKAN>>(std::move(command_buffer));
    }

    DebugLog(LogType::RenDebug, "Allocated %d command buffers\n", m_per_frame_data.NumFrames());

    return result;
}

template <>
Result FrameHandler<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    auto result = Result::OK;

    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<CommandBuffer<Platform::VULKAN>>()->Destroy(device), result);
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<Frame<Platform::VULKAN>>()->Destroy(device), result);
    }

    m_per_frame_data.Reset();

    return result;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion

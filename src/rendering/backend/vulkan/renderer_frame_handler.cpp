#include "renderer_frame_handler.h"

#include <math/math_util.h>
#include <constants.h>

namespace hyperion {
namespace renderer {

FrameHandler::FrameHandler(UInt num_frames, NextImageFunction next_image)
    : m_per_frame_data(MathUtil::Min(num_frames, max_frames_in_flight)),
      m_next_image(next_image),
      m_acquired_image_index(0),
      m_current_frame_index(0)
{
}

FrameHandler::~FrameHandler() = default;

Result FrameHandler::CreateFrames(Device *device)
{
    AssertThrow(m_per_frame_data.NumFrames() >= 1);
    
    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        auto frame = std::make_unique<Frame>(i);

        HYPERION_BUBBLE_ERRORS(frame->Create(
            device,
            non_owning_ptr(m_per_frame_data[i].Get<CommandBuffer>())
        ));

        m_per_frame_data[i].Set<Frame>(std::move(frame));
    }

    DebugLog(LogType::Debug, "Allocated [%d] frames\n", m_per_frame_data.NumFrames());

    HYPERION_RETURN_OK;
}

Result FrameHandler::PrepareFrame(Device *device, Swapchain *swapchain)
{
    auto *frame = GetCurrentFrameData().Get<Frame>();

    VkResult fence_result;

    frame->fc_queue_submit->WaitForGpu(device, true, &fence_result);

    if (fence_result == VK_SUBOPTIMAL_KHR || fence_result == VK_ERROR_OUT_OF_DATE_KHR) {
        DebugLog(LogType::Debug, "Waiting -- image result was %d\n", fence_result);

        vkDeviceWaitIdle(device->GetDevice());

        /* TODO: regenerate framebuffers and swapchain */
    }

    HYPERION_VK_CHECK(fence_result);
    HYPERION_BUBBLE_ERRORS(frame->fc_queue_submit->Reset(device));

    HYPERION_VK_CHECK(m_next_image(device, swapchain, frame, &m_acquired_image_index));

    HYPERION_RETURN_OK;
}

void FrameHandler::NextFrame()
{
    m_current_frame_index = (m_current_frame_index + 1) % NumFrames();
}

Result FrameHandler::PresentFrame(Queue *queue, Swapchain *swapchain) const
{
    auto *frame = GetCurrentFrameData().Get<Frame>();

    const auto &signal_semaphores = frame->GetPresentSemaphores().GetSignalSemaphoresView();

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};

    present_info.waitSemaphoreCount = static_cast<UInt>(signal_semaphores.size());
    present_info.pWaitSemaphores    = signal_semaphores.data();

    present_info.swapchainCount = 1;
    present_info.pSwapchains    = &swapchain->swapchain;
    present_info.pImageIndices  = &m_acquired_image_index;
    present_info.pResults       = nullptr;

    HYPERION_VK_CHECK(vkQueuePresentKHR(queue->queue, &present_info));

    HYPERION_RETURN_OK;
}


Result FrameHandler::CreateCommandBuffers(Device *device, VkCommandPool pool)
{
    auto result = Result::OK;

    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        auto command_buffer = std::make_unique<CommandBuffer>(
            CommandBuffer::COMMAND_BUFFER_PRIMARY
        );

        HYPERION_PASS_ERRORS(command_buffer->Create(device, pool), result);

        m_per_frame_data[i].Set<CommandBuffer>(std::move(command_buffer));
    }

    DebugLog(LogType::Debug, "Allocated %d command buffers\n", m_per_frame_data.NumFrames());

    return result;
}

Result FrameHandler::Destroy(Device *device, VkCommandPool pool)
{
    auto result = Result::OK;

    for (UInt i = 0; i < m_per_frame_data.NumFrames(); i++) {
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<CommandBuffer>()->Destroy(device, pool), result);
        HYPERION_PASS_ERRORS(m_per_frame_data[i].Get<Frame>()->Destroy(device), result);
    }

    m_per_frame_data.Reset();

    return result;
}

} // namespace renderer
} // namespace hyperion

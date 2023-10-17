#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererQueue.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
Frame<Platform::VULKAN>::Frame()
    : m_frame_index(0),
      m_command_buffer(nullptr),
      fc_queue_submit(nullptr),
      m_present_semaphores({}, {})
{
}

template <>
Frame<Platform::VULKAN>::Frame(UInt frame_index)
    : m_frame_index(frame_index),
      m_command_buffer(nullptr),
      fc_queue_submit(nullptr),
      m_present_semaphores(
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
          { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
      )
{
}

template <>
Frame<Platform::VULKAN>::Frame(Frame &&other) noexcept
    : m_frame_index(other.m_frame_index),
      m_command_buffer(other.m_command_buffer),
      fc_queue_submit(std::move(other.fc_queue_submit)),
      m_present_semaphores(std::move(other.m_present_semaphores))
{
    other.m_frame_index = 0;
    other.m_command_buffer = nullptr;
}

template <>
Frame<Platform::VULKAN>::~Frame()
{
    AssertThrowMsg(fc_queue_submit == nullptr, "fc_queue_submit should have been destroyed");
}

template <>
Result Frame<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, CommandBuffer<Platform::VULKAN> *cmd)
{
    m_command_buffer = cmd;
    
    HYPERION_BUBBLE_ERRORS(m_present_semaphores.Create(device));

    fc_queue_submit = std::make_unique<Fence>(true);

    HYPERION_BUBBLE_ERRORS(fc_queue_submit->Create(device));

    DebugLog(LogType::Debug, "Create Sync objects!\n");

    HYPERION_RETURN_OK;
}

template <>
Result Frame<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    auto result = Result::OK;

    HYPERION_PASS_ERRORS(m_present_semaphores.Destroy(device), result);

    AssertThrow(fc_queue_submit != nullptr);
    HYPERION_PASS_ERRORS(fc_queue_submit->Destroy(device), result);
    fc_queue_submit = nullptr;

    return result;
}

template <>
Result Frame<Platform::VULKAN>::BeginCapture(Device<Platform::VULKAN> *device)
{
    return m_command_buffer->Begin(device);
}

template <>
Result Frame<Platform::VULKAN>::EndCapture(Device<Platform::VULKAN> *device)
{
    return m_command_buffer->End(device);
}

template <>
Result Frame<Platform::VULKAN>::Submit(DeviceQueue *queue)
{
    return m_command_buffer->SubmitPrimary(
        queue->queue,
        fc_queue_submit.get(),
        &m_present_semaphores
    );
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <util/Defines.hpp>

#include <Types.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrame.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

class Swapchain;
class CommandBuffer;
class Fence;
class DeviceQueue;

using ::hyperion::non_owning_ptr;
class Frame
{
public:
    static Frame TemporaryFrame(CommandBuffer *command_buffer, UInt frame_index = 0);

    explicit Frame();
    Frame(UInt frame_index);
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    Frame(Frame &&other) noexcept;
    ~Frame();

    Result Create(Device *device, const non_owning_ptr<CommandBuffer> &cmd);
    Result Destroy(Device *device);

    HYP_FORCE_INLINE UInt GetFrameIndex() const { return m_frame_index; }

    CommandBuffer *GetCommandBuffer() { return command_buffer.get(); }
    const CommandBuffer *GetCommandBuffer() const { return command_buffer.get(); }

    SemaphoreChain &GetPresentSemaphores() { return m_present_semaphores; }
    const SemaphoreChain &GetPresentSemaphores() const { return m_present_semaphores; }

    /* Start recording into the command buffer */
    Result BeginCapture(Device *device);
    /* Stop recording into the command buffer */
    Result EndCapture(Device *device);
    /* Submit command buffer to the given queue */
    Result Submit(DeviceQueue *queue);
    
    non_owning_ptr<CommandBuffer> command_buffer;
    std::unique_ptr<Fence> fc_queue_submit;

private:
    UInt m_frame_index;
    SemaphoreChain m_present_semaphores;
};

} // namespace renderer
} // namespace hyperion

#endif
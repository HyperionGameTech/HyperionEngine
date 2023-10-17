#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <util/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class Fence;
struct DeviceQueue;

using ::hyperion::non_owning_ptr;

namespace platform {

template <PlatformType PLATFORM>
class Swapchain;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Frame
{
public:
    static Frame TemporaryFrame(CommandBuffer<PLATFORM> *command_buffer, UInt frame_index = 0)
    {
        Frame frame;
        frame.m_command_buffer = command_buffer;
        frame.m_frame_index = frame_index;
        return frame;
    }

    explicit Frame();
    Frame(UInt frame_index);
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    Frame(Frame &&other) noexcept;
    ~Frame();

    Result Create(Device<PLATFORM> *device, CommandBuffer<PLATFORM> *cmd);
    Result Destroy(Device<PLATFORM> *device);

    HYP_FORCE_INLINE UInt GetFrameIndex() const
        { return m_frame_index; }

    CommandBuffer<PLATFORM> *GetCommandBuffer() const
        { return m_command_buffer; }

    SemaphoreChain &GetPresentSemaphores()
        { return m_present_semaphores; }

    const SemaphoreChain &GetPresentSemaphores() const
        { return m_present_semaphores; }

    /* Start recording into the command buffer */
    Result BeginCapture(Device<PLATFORM> *device);
    /* Stop recording into the command buffer */
    Result EndCapture(Device<PLATFORM> *device);
    /* Submit command buffer to the given queue */
    Result Submit(DeviceQueue *queue);
    
    // Not owned
    CommandBuffer<PLATFORM> *m_command_buffer;
    std::unique_ptr<Fence>  fc_queue_submit;

private:
    UInt            m_frame_index;
    SemaphoreChain  m_present_semaphores;
};

} // namespace platform

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrame.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using Frame = platform::Frame<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
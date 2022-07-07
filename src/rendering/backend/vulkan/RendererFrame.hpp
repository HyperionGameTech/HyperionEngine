#ifndef HYPERION_RENDERER_FRAME_H
#define HYPERION_RENDERER_FRAME_H

#include "RendererResult.hpp"
#include "RendererDevice.hpp"
#include "RendererCommandBuffer.hpp"
#include "RendererFence.hpp"
#include "RendererSemaphore.hpp"
#include "RendererQueue.hpp"

#include <util/NonOwningPtr.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class Swapchain;

using ::hyperion::non_owning_ptr;

class Frame {
public:
    static Frame TemporaryFrame(CommandBuffer *command_buffer, uint32_t frame_index);

    Frame(uint32_t frame_index);
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    Frame(Frame &&other) noexcept;
    ~Frame();

    Result Create(Device *device, const non_owning_ptr<CommandBuffer> &cmd);
    Result Destroy(Device *device);

    uint32_t GetFrameIndex() const                       { return m_frame_index; }

    inline CommandBuffer *GetCommandBuffer()             { return command_buffer.get(); }
    inline const CommandBuffer *GetCommandBuffer() const { return command_buffer.get(); }

    inline SemaphoreChain &GetPresentSemaphores()             { return m_present_semaphores; }
    inline const SemaphoreChain &GetPresentSemaphores() const { return m_present_semaphores; }

    /* Start recording into the command buffer */
    Result BeginCapture(Device *device);
    /* Stop recording into the command buffer */
    Result EndCapture(Device *device);
    /* Submit command buffer to the given queue */
    Result Submit(Queue *queue);
    
    non_owning_ptr<CommandBuffer> command_buffer;
    std::unique_ptr<Fence>        fc_queue_submit;

private:
    uint32_t       m_frame_index;
    SemaphoreChain m_present_semaphores;
};

} // namespace renderer
} // namespace hyperion

#endif

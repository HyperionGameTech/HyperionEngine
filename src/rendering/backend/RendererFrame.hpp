/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Fence;

template <PlatformType PLATFORM>
class Swapchain;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct DeviceQueue;

template <PlatformType PLATFORM>
class Frame
{
public:
    static Frame TemporaryFrame(CommandBufferRef<PLATFORM> command_buffer, uint frame_index = 0)
    {
        Frame frame;
        frame.m_command_buffer = std::move(command_buffer);
        frame.m_frame_index = frame_index;
        return frame;
    }

    explicit Frame();
    Frame(uint frame_index);
    Frame(const Frame &other) = delete;
    Frame &operator=(const Frame &other) = delete;
    Frame(Frame &&other) noexcept;
    ~Frame();

    Result Create(Device<PLATFORM> *device, CommandBufferRef<PLATFORM> cmd);
    Result Destroy(Device<PLATFORM> *device);

    const FenceRef<PLATFORM> &GetFence() const
        { return m_queue_submit_fence; }

    HYP_FORCE_INLINE uint GetFrameIndex() const
        { return m_frame_index; }

    const CommandBufferRef<PLATFORM> &GetCommandBuffer() const
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
    Result Submit(DeviceQueue<PLATFORM> *queue);
    
    CommandBufferRef<PLATFORM>  m_command_buffer;

private:
    uint                m_frame_index;
    SemaphoreChain      m_present_semaphores;
    FenceRef<PLATFORM>  m_queue_submit_fence;
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
/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FRAME_HPP
#define HYPERION_BACKEND_RENDERER_FRAME_HPP

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <rendering/rhi/RHICommandList.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class IFrame
{
public:
    virtual ~IFrame() = default;

    virtual uint32 GetFrameIndex() const = 0;
    virtual RHICommandList &GetCommandList() = 0;
    virtual const RHICommandList &GetCommandList() const = 0;
};

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
class Frame final : public IFrame
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    static FrameRef<PLATFORM> TemporaryFrame(uint32 frame_index = 0)
    {
        FrameRef<PLATFORM> frame = MakeRenderObject<Frame<PLATFORM>>();
        frame->m_frame_index = frame_index;
        return frame;
    }

    explicit Frame();
    Frame(uint32 frame_index);
    Frame(const Frame &other)                   = delete;
    Frame &operator=(const Frame &other)        = delete;
    Frame(Frame &&other) noexcept               = delete;
    Frame &operator=(Frame &&other) noexcept    = delete;
    virtual ~Frame() override;

    RendererResult Create();
    RendererResult Destroy();

    virtual uint32 GetFrameIndex() const override
        { return m_frame_index; }
    
    virtual RHICommandList &GetCommandList() override
        { return m_command_list; }
    
    virtual const RHICommandList &GetCommandList() const override
        { return m_command_list; }

    HYP_FORCE_INLINE const FenceRef<PLATFORM> &GetFence() const
        { return m_queue_submit_fence; }

    HYP_FORCE_INLINE SemaphoreChain &GetPresentSemaphores()
        { return m_present_semaphores; }

    HYP_FORCE_INLINE const SemaphoreChain &GetPresentSemaphores() const
        { return m_present_semaphores; }

    RendererResult RecreateFence();

private:
    uint32              m_frame_index;
    SemaphoreChain      m_present_semaphores;
    FenceRef<PLATFORM>  m_queue_submit_fence;
    RHICommandList      m_command_list;
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
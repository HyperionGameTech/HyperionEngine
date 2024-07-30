/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_FRAME_HANDLER_HPP
#define HYPERION_BACKEND_RENDERER_FRAME_HANDLER_HPP

#include <core/containers/FixedArray.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>
#include <Types.hpp>
#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Swapchain;

template <PlatformType PLATFORM>
struct DeviceQueue;

template <PlatformType PLATFORM>
class FrameHandler
{
public:
    static constexpr PlatformType platform = PLATFORM;
    
    using NextImageFunction = std::add_pointer_t<Result(Device<PLATFORM> *device, Swapchain<PLATFORM> *swapchain, Frame<PLATFORM> *frame, uint *image_index)>;

    FrameHandler(uint num_frames, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other)                 = delete;
    FrameHandler &operator=(const FrameHandler &other)      = delete;
    FrameHandler(FrameHandler &&other) noexcept             = delete;
    FrameHandler &operator=(FrameHandler &&other) noexcept  = delete;
    ~FrameHandler()                                         = default;

    HYP_FORCE_INLINE const FrameRef<PLATFORM> &GetCurrentFrame() const
        { return m_frames[m_current_frame_index]; }

    HYP_FORCE_INLINE uint GetAcquiredImageIndex() const
        { return m_acquired_image_index; }

    HYP_FORCE_INLINE uint GetCurrentFrameIndex() const
        { return m_current_frame_index; }

    /* Used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    HYP_API Result PrepareFrame(Device<PLATFORM> *device, Swapchain<PLATFORM> *swapchain);
    /* Submit the frame for presentation */
    HYP_API Result PresentFrame(DeviceQueue<PLATFORM> *queue, Swapchain<PLATFORM> *swapchain) const;
    /* Advance the current frame index; call at the end of a render loop. */
    HYP_API void NextFrame();
    /* Create our Frame objects (count is same as num_frames) */
    HYP_API Result CreateFrames(Device<PLATFORM> *device, DeviceQueue<PLATFORM> *queue);
    HYP_API Result Destroy(Device<PLATFORM> *device);

private:
    FixedArray<FrameRef<PLATFORM>, max_frames_in_flight>    m_frames;
    NextImageFunction                                       m_next_image;
    uint                                                    m_acquired_image_index;
    uint                                                    m_current_frame_index;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrameHandler.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using FrameHandler = platform::FrameHandler<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif
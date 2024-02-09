#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H

#include <core/lib/FixedArray.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

class DeviceQueue;

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
class FrameHandler
{
public:
    using NextImageFunction = std::add_pointer_t<Result(Device<PLATFORM> *device, Swapchain<PLATFORM> *swapchain, Frame<PLATFORM> *frame, uint *image_index)>;

    FrameHandler(uint num_frames, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    const FrameRef<PLATFORM> &GetCurrentFrame() const
        { return m_frames[m_current_frame_index]; }

    uint GetAcquiredImageIndex() const
        { return m_acquired_image_index; }

    uint GetCurrentFrameIndex() const
        { return m_current_frame_index; }

    /* Used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    Result PrepareFrame(Device<PLATFORM> *device, Swapchain<PLATFORM> *swapchain);
    /* Submit the frame for presentation */
    Result PresentFrame(DeviceQueue *queue, Swapchain<PLATFORM> *swapchain) const;
    /* Advance the current frame index; call at the end of a render loop. */
    void NextFrame();
    /* Create our Frame objects (count is same as num_frames) */
    Result CreateFrames(Device<PLATFORM> *device, DeviceQueue *queue);
    Result Destroy(Device<PLATFORM> *device);

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
#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
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
    using NextImageFunction = std::add_pointer_t<Result(Device<PLATFORM> *device, Swapchain<PLATFORM> *swapchain, Frame<PLATFORM> *frame, UInt *image_index)>;

    FrameHandler(UInt num_frames, NextImageFunction next_image);
    FrameHandler(const FrameHandler &other) = delete;
    FrameHandler &operator=(const FrameHandler &other) = delete;
    ~FrameHandler();

    auto &GetPerFrameData() { return m_per_frame_data; }
    const auto &GetPerFrameData() const { return m_per_frame_data; }

    auto &GetCurrentFrameData() { return m_per_frame_data[m_current_frame_index]; }
    const auto &GetCurrentFrameData() const { return m_per_frame_data[m_current_frame_index]; }

    UInt NumFrames() const { return m_per_frame_data.NumFrames(); }
    UInt GetAcquiredImageIndex() const { return m_acquired_image_index; }
    UInt GetCurrentFrameIndex() const { return m_current_frame_index; }

    /* Used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    Result PrepareFrame(Device<Platform::VULKAN> *device, Swapchain<Platform::VULKAN> *swapchain);
    /* Submit the frame for presentation */
    Result PresentFrame(DeviceQueue *queue, Swapchain<Platform::VULKAN> *swapchain) const;
    /* Advance the current frame index; call at the end of a render loop. */
    void NextFrame();
    /* Create our Frame objects (count is same as num_frames) */
    Result CreateFrames(Device<Platform::VULKAN> *device);
    /* Create our CommandBuffer objects (count is same as num_frames) */
    Result CreateCommandBuffers(Device<Platform::VULKAN> *device, DeviceQueue *queue);
    Result Destroy(Device<Platform::VULKAN> *device);

private:
    PerFrameData<CommandBuffer<Platform::VULKAN>, Frame<Platform::VULKAN>>  m_per_frame_data;
    NextImageFunction                                                       m_next_image;
    UInt                                                                    m_acquired_image_index;
    UInt                                                                    m_current_frame_index;
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
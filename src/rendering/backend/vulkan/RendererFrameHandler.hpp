#ifndef HYPERION_RENDERER_FRAME_HANDLER_H
#define HYPERION_RENDERER_FRAME_HANDLER_H

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererQueue.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class FrameHandler {
public:
    using NextImageFunction = std::add_pointer_t<VkResult(Device *device, Swapchain *swapchain, Frame *frame, UInt *image_index)>;

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
    Result PrepareFrame(Device *device, Swapchain *swapchain);
    /* Submit the frame for presentation */
    Result PresentFrame(DeviceQueue *queue, Swapchain *swapchain) const;
    /* Advance the current frame index; call at the end of a render loop. */
    void NextFrame();
    /* Create our Frame objects (count is same as num_frames) */
    Result CreateFrames(Device *device);
    /* Create our CommandBuffer objects (count is same as num_frames) */
    Result CreateCommandBuffers(Device *device, VkCommandPool pool);
    Result Destroy(Device *device, VkCommandPool pool);

private:
    PerFrameData<CommandBuffer, Frame> m_per_frame_data;

    NextImageFunction m_next_image;
    UInt m_acquired_image_index;
    UInt m_current_frame_index;
};

} // namespace renderer
} // namespace hyperion

#endif
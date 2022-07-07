#ifndef HYPERION_RENDERER_FRAME_HANDLER_H
#define HYPERION_RENDERER_FRAME_HANDLER_H

#include "RendererFrame.hpp"
#include "RendererSwapchain.hpp"
#include "RendererCommandBuffer.hpp"
#include "RendererStructs.hpp"
#include "RendererQueue.hpp"

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

    inline auto &GetPerFrameData() { return m_per_frame_data; }
    inline const auto &GetPerFrameData() const { return m_per_frame_data; }

    inline auto &GetCurrentFrameData() { return m_per_frame_data[m_current_frame_index]; }
    inline const auto &GetCurrentFrameData() const { return m_per_frame_data[m_current_frame_index]; }

    inline UInt NumFrames() const { return m_per_frame_data.NumFrames(); }
    inline UInt GetAcquiredImageIndex() const { return m_acquired_image_index; }
    inline UInt GetCurrentFrameIndex() const { return m_current_frame_index; }

    /* Used to acquire a new image from the provided next_image function.
     * Really only useful for our main swapchain surface */
    Result PrepareFrame(Device *device, Swapchain *swapchain);
    /* Submit the frame for presentation */
    Result PresentFrame(Queue *queue, Swapchain *swapchain) const;
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
/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Memory/Pool/Pool.hpp>
#include <Core/Memory/SharedPtr.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class Texture;

/*! \brief Offscreen capture target for a single asset thumbnail.
 *
 *  The owning system points a View with ViewFlags::THUMBNAIL_VIEW at one of these, arms it with
 *  Request(), and receives the finished pixels once the GPU is done with the frame that drew them.
 *  DeferredPass drives CaptureFrom() as it finishes rendering the View.
 *
 *  Two images come back, both tightly packed RGBA16F at the capture extent:
 *  - colour, straight off the tonemap pass, and so in linear space. The swapchain is an sRGB format, so
 *    the display encode normally happens in hardware; callers writing to an 8-bit file must apply the
 *    sRGB transfer function themselves.
 *  - coverage, the GBuffer albedo target, whose alpha is zero where nothing was rasterised. The tonemap
 *    pass hardcodes alpha to 1, so this is the only way to tell the subject from the background. */
struct ENGINE_API ThumbnailCaptureState
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    using Callback = Proc<void(ByteBuffer&& color, ByteBuffer&& coverage, Vec2u extent)>;

    explicit ThumbnailCaptureState(Vec2u extent);
    ~ThumbnailCaptureState();

    ThumbnailCaptureState(const ThumbnailCaptureState& other) = delete;
    ThumbnailCaptureState& operator=(const ThumbnailCaptureState& other) = delete;

    HYP_FORCE_INLINE Vec2u GetExtent() const
    {
        return m_extent;
    }

    /*! \brief Arm the capture. The next frame rendered for the associated View is copied off and
     *  delivered to \p callback on the render thread. Any previously armed request is replaced. */
    void Request(Callback&& callback);

    HYP_FORCE_INLINE bool IsRequested() const
    {
        return m_isRequested.Get(MemoryOrder::ACQUIRE);
    }

    /*! \brief Render thread. Copies the given views into the capture textures and starts the readbacks.
     *  Does nothing unless a capture has been armed with Request(). */
    void CaptureFrom(Frame* frame, const GpuImageViewRef& colorImageView, const GpuImageViewRef& coverageImageView);

private:
    /*! \brief Joins the colour and coverage readbacks, which retire independently. */
    struct PendingCapture
    {
        Mutex mutex;
        ByteBuffer color;
        ByteBuffer coverage;
        Vec2u extent;
        AtomicVar<int32> remaining { 2 };
        Callback callback;
    };

    void CopyInto(Frame* frame, const GpuImageViewRef& srcImageView, const Handle<Texture>& dstTexture);

    void EnqueueReadbackInto(
        const Handle<Texture>& texture,
        const SharedPtr<PendingCapture>& pending,
        bool isCoverage);

    Vec2u m_extent;

    Handle<Texture> m_texture;
    Handle<Texture> m_coverageTexture;

    Mutex m_callbackMutex;
    Callback m_callback;

    AtomicVar<bool> m_isRequested { false };
};

} // namespace Hyperion

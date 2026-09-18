/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/ThumbnailCaptureState.hpp>

#include <Rendering/CommandRecorder.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

namespace Hyperion {

static Handle<Texture> CreateCaptureTexture(Vec2u extent, Name name)
{
    // Matches the tonemap pass and GBuffer albedo formats, so a capture is a straight image copy with no
    // format conversion. Nearest filtering and a single mip - nothing samples this, it is only read back.
    Handle<Texture> texture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        TextureFormat::RGBA16F,
        Vec3u { extent, 1 },
        TextureFilterMode::Nearest,
        TextureFilterMode::Nearest,
        TextureWrapMode::ClampToEdge,
        1,
        ImageUsage::Sampled | ImageUsage::Attachment });

    texture->SetName(name);
    texture->SetIsTransient(true);

    Check(texture->Create());

    return texture;
}

ThumbnailCaptureState::ThumbnailCaptureState(Vec2u extent)
    : m_extent(extent)
{
    m_texture = CreateCaptureTexture(extent, NAME("ThumbnailCaptureTarget"));
    m_coverageTexture = CreateCaptureTexture(extent, NAME("ThumbnailCaptureCoverage"));
}

ThumbnailCaptureState::~ThumbnailCaptureState()
{
    if (m_texture.IsValid())
    {
        EnqueueDeletion(std::move(m_texture));
    }

    if (m_coverageTexture.IsValid())
    {
        EnqueueDeletion(std::move(m_coverageTexture));
    }
}

void ThumbnailCaptureState::Request(Callback&& callback)
{
    {
        Mutex::Guard guard(m_callbackMutex);
        m_callback = std::move(callback);
    }

    m_isRequested.Set(true, MemoryOrder::RELEASE);
}

void ThumbnailCaptureState::CopyInto(Frame* frame, const GpuImageViewRef& srcImageView, const Handle<Texture>& dstTexture)
{
    const GpuImageRef& srcImage = srcImageView->GetImage();
    const GpuImageRef& dstImage = dstTexture->GetGpuImage();

    const ResourceState previousSrcState = srcImage->GetResourceState();

    CommandRecorder& cr = frame->cr;

    cr << InsertBarrier(srcImage, ResourceState::CopySrc);
    cr << InsertBarrier(dstImage, ResourceState::CopyDst);

    cr << CopyImage(srcImage, dstImage, Vec3u(m_extent, 1));

    cr << InsertBarrier(dstImage, ResourceState::ShaderResource);

    if (previousSrcState != ResourceState::Undefined && previousSrcState != ResourceState::PreInitialized)
    {
        cr << InsertBarrier(srcImage, previousSrcState);
    }
    else
    {
        cr << InsertBarrier(srcImage, ResourceState::ShaderResource);
    }
}

void ThumbnailCaptureState::EnqueueReadbackInto(
    const Handle<Texture>& texture,
    const SharedPtr<PendingCapture>& pending,
    bool isCoverage)
{
    const size_t expectedSize = size_t(m_extent.x) * size_t(m_extent.y) * 8; // RGBA16F

    texture->EnqueueReadback(
        [pending, expectedSize, isCoverage](GpuBuffer& buffer)
        {
            if (buffer.Size() >= expectedSize)
            {
                ByteBuffer pixels(expectedSize, buffer.Map());

                buffer.Unmap();

                Mutex::Guard guard(pending->mutex);

                if (isCoverage)
                {
                    pending->coverage = std::move(pixels);
                }
                else
                {
                    pending->color = std::move(pixels);
                }
            }
            else
            {
                HYP_LOG(Rendering, Warning, "Thumbnail readback returned {} bytes, expected at least {}.",
                    buffer.Size(), expectedSize);
            }

            // The two readbacks retire independently; whichever finishes last delivers the pair.
            if (pending->remaining.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1 > 0)
            {
                return;
            }

            if (pending->color.Size() == 0 || pending->coverage.Size() == 0)
            {
                return;
            }

            pending->callback(std::move(pending->color), std::move(pending->coverage), pending->extent);
        });
}

void ThumbnailCaptureState::CaptureFrom(Frame* frame, const GpuImageViewRef& colorImageView, const GpuImageViewRef& coverageImageView)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!m_isRequested.Get(MemoryOrder::ACQUIRE))
    {
        return;
    }

    if (!colorImageView.IsValid() || !colorImageView->GetImage().IsValid()
        || !coverageImageView.IsValid() || !coverageImageView->GetImage().IsValid())
    {
        return;
    }

    if (!m_texture.IsValid() || !m_texture->IsCreated()
        || !m_coverageTexture.IsValid() || !m_coverageTexture->IsCreated())
    {
        HYP_LOG(Rendering, Warning, "Thumbnail capture targets are not ready, skipping capture.");

        return;
    }

    Callback callback;

    {
        Mutex::Guard guard(m_callbackMutex);
        callback = std::move(m_callback);
        m_callback = Callback();
    }

    // Only capture once per Request(), even if more frames are rendered for the View.
    m_isRequested.Set(false, MemoryOrder::RELEASE);

    if (!callback.IsValid())
    {
        return;
    }

    CopyInto(frame, colorImageView, m_texture);
    CopyInto(frame, coverageImageView, m_coverageTexture);

    SharedPtr<PendingCapture> pending = MakeShared<PendingCapture>();
    pending->extent = m_extent;
    pending->callback = std::move(callback);

    EnqueueReadbackInto(m_texture, pending, /* isCoverage */ false);
    EnqueueReadbackInto(m_coverageTexture, pending, /* isCoverage */ true);
}

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Preview/MaterialPreviewRenderer.hpp>
#include <Editor/Preview/AssetThumbnailService.hpp>

#include <Asset/AssetBucket.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Rendering/Material.hpp>

#include <Scene/World.hpp>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

static constexpr uint32 SettleFrames = 2;
static constexpr uint32 CaptureTimeoutFrames = 240;

MaterialPreviewRenderer::MaterialPreviewRenderer() = default;

MaterialPreviewRenderer::~MaterialPreviewRenderer()
{
    Shutdown();
}

void MaterialPreviewRenderer::Initialize(World* world)
{
    AssertOnThread(g_simThread);

    m_world = world;
    m_resultSink = MakeShared<ResultSink>();

    m_previewScene = MakeUnique<AssetPreviewScene>(NAME("MaterialPreview"), Vec2u(PreviewSize, PreviewSize));
}

void MaterialPreviewRenderer::Shutdown()
{
    m_isActive = false;
    m_bucketIndex = 0;
    m_assetName = Name();

    m_stage = CaptureStage::Idle;
    m_framesInStage = 0;
    m_isDirty.Set(false, MemoryOrder::RELEASE);

    m_resultSink.Reset();

    {
        Mutex::Guard guard(m_frameMutex);

        m_latestFrame = ByteBuffer();
        m_latestExtent = Vec2u::Zero();
    }

    if (m_previewScene)
    {
        m_previewScene->Shutdown();
        m_previewScene.Reset();
    }

    m_world = nullptr;
}

void MaterialPreviewRenderer::SetMaterial(uint32 bucketIndex, Name assetName)
{
    AssertOnThread(g_simThread);

    const bool isValidRequest = bucketIndex == AssetBuckets::Materials.GetIndex() && assetName.IsValid();

    if (!isValidRequest)
    {
        m_isActive = false;
        m_bucketIndex = 0;
        m_assetName = Name();

        m_stage = CaptureStage::Idle;
        m_framesInStage = 0;

        {
            Mutex::Guard guard(m_frameMutex);

            m_latestFrame = ByteBuffer();
            m_latestExtent = Vec2u::Zero();
        }

        return;
    }

    m_bucketIndex = bucketIndex;
    m_assetName = assetName;
    m_isActive = true;

    // A different material means the frame we are holding is for the wrong asset.
    {
        Mutex::Guard guard(m_frameMutex);

        m_latestFrame = ByteBuffer();
        m_latestExtent = Vec2u::Zero();
    }

    m_stage = CaptureStage::Idle;
    m_framesInStage = 0;

    Invalidate();
}

void MaterialPreviewRenderer::SetLightAngles(float yaw, float pitch)
{
    AssertOnThread(g_simThread);

    // Pitch is clamped short of straight up/down so the light never degenerates onto the view axis.
    m_lightYaw = yaw;
    m_lightPitch = MathUtil::Clamp(pitch, MathUtil::DegToRad(-80.0f), MathUtil::DegToRad(80.0f));

    ApplyLightAngles();

    Invalidate();
}

void MaterialPreviewRenderer::ApplyLightAngles()
{
    if (!m_previewScene || !m_previewScene->IsInitialized())
    {
        return;
    }

    m_previewScene->SetKeyLightViewAngles(m_lightYaw, m_lightPitch);
}

void MaterialPreviewRenderer::Invalidate()
{
    m_isDirty.Set(true, MemoryOrder::RELEASE);
}

bool MaterialPreviewRenderer::BeginRender()
{
    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry)
    {
        return false;
    }

    Handle<Material> material = registry->GetAsset<Material>(AssetBucket(m_bucketIndex), m_assetName);

    if (!material.IsValid())
    {
        return false;
    }

    if (!m_previewScene->Initialize(m_world))
    {
        return false;
    }

    m_previewScene->ShowMaterial(material.Get());

    ApplyLightAngles();

    return true;
}

void MaterialPreviewRenderer::Update()
{
    AssertOnThread(g_simThread);

    if (!m_world || !m_previewScene || !m_isActive)
    {
        return;
    }

    if (m_stage == CaptureStage::Idle)
    {
        if (!m_isDirty.Get(MemoryOrder::ACQUIRE))
        {
            return;
        }

        m_isDirty.Set(false, MemoryOrder::RELEASE);

        if (!BeginRender())
        {
            return;
        }

        m_stage = CaptureStage::Settling;
        m_framesInStage = 0;

        return;
    }

    m_previewScene->Submit();

    m_framesInStage++;

    if (m_stage == CaptureStage::Settling)
    {
        if (m_framesInStage >= SettleFrames)
        {
            if (!m_resultSink)
            {
                m_stage = CaptureStage::Idle;
                m_framesInStage = 0;

                return;
            }

            m_stage = CaptureStage::Capturing;
            m_framesInStage = 0;

            m_previewScene->RequestCapture(
                [sink = m_resultSink](ByteBuffer&& pixels, ByteBuffer&& coverage, Vec2u extent)
                {
                    Mutex::Guard guard(sink->mutex);

                    sink->pixels = std::move(pixels);
                    sink->coverage = std::move(coverage);
                    sink->extent = extent;
                    sink->hasResult = true;
                });
        }

        return;
    }

    ByteBuffer pixels;
    ByteBuffer coverage;
    Vec2u extent;
    bool hasResult = false;

    {
        Mutex::Guard guard(m_resultSink->mutex);

        if (m_resultSink->hasResult)
        {
            pixels = std::move(m_resultSink->pixels);
            coverage = std::move(m_resultSink->coverage);
            extent = m_resultSink->extent;
            hasResult = true;

            m_resultSink->pixels = ByteBuffer();
            m_resultSink->coverage = ByteBuffer();
            m_resultSink->hasResult = false;
        }
    }

    if (hasResult)
    {
        m_stage = CaptureStage::Idle;
        m_framesInStage = 0;

        ByteBuffer rgba8;

        if (ConvertPreviewPixelsToRgba8(pixels, coverage, extent, rgba8))
        {
            {
                Mutex::Guard guard(m_frameMutex);

                m_latestFrame = std::move(rgba8);
                m_latestExtent = extent;
            }

            OnFrameReady();
        }

        return;
    }

    if (m_framesInStage >= CaptureTimeoutFrames)
    {
        HYP_LOG(Editor, Warning, "Timed out capturing material preview for '{}'.", m_assetName);

        m_stage = CaptureStage::Idle;
        m_framesInStage = 0;
    }
}

size_t MaterialPreviewRenderer::CopyLatestFrame(void* dest, size_t destSize, uint32& outWidth, uint32& outHeight) const
{
    Mutex::Guard guard(m_frameMutex);

    outWidth = m_latestExtent.x;
    outHeight = m_latestExtent.y;

    if (m_latestFrame.Size() == 0)
    {
        return 0;
    }

    if (!dest || destSize < m_latestFrame.Size())
    {
        return 0;
    }

    std::memcpy(dest, m_latestFrame.Data(), m_latestFrame.Size());

    return m_latestFrame.Size();
}

} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/Preview/AssetThumbnailService.hpp>

#include <Asset/AssetBucket.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/Float16.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Rendering/Material.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>

#include <Scene/Prefab.hpp>
#include <Scene/World.hpp>

#include <stb_image_write.h>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

//!< Frames rendered after posing before the capture is armed, so entity collection, shader compilation
//!< and GPU uploads have a chance to land. Capturing sooner yields a blank image.
static constexpr uint32 SettleFrames = 4;

//!< Gives up on a capture that never completes rather than wedging the queue forever.
static constexpr uint32 CaptureTimeoutFrames = 240;

bool ConvertPreviewPixelsToRgba8(
    const ByteBuffer& linearRgba16f,
    const ByteBuffer& coverageRgba16f,
    Vec2u extent,
    ByteBuffer& outRgba8)
{
    const uint32 numPixels = extent.x * extent.y;

    if (numPixels == 0
        || linearRgba16f.Size() < size_t(numPixels) * 8
        || coverageRgba16f.Size() < size_t(numPixels) * 8)
    {
        return false;
    }

    outRgba8 = ByteBuffer(size_t(numPixels) * 4);

    const Float16* src = reinterpret_cast<const Float16*>(linearRgba16f.Data());
    const Float16* coverage = reinterpret_cast<const Float16*>(coverageRgba16f.Data());
    ubyte* dst = outRgba8.Data();

    for (uint32 i = 0; i < numPixels; i++)
    {
        // The GBuffer albedo target is cleared to zero and the geometry pass always writes a non-zero
        // alpha, so anything still at zero is background the subject did not cover. Previews are cut out
        // rather than showing whatever the pipeline painted behind them.
        const bool isCovered = float(coverage[i * 4 + 3]) > 0.0f;

        for (uint32 c = 0; c < 3; c++)
        {
            float value = MathUtil::Clamp(float(src[i * 4 + c]), 0.0f, 1.0f);

            value = value <= 0.0031308f
                ? value * 12.92f
                : 1.055f * MathUtil::Pow(value, 1.0f / 2.4f) - 0.055f;

            dst[i * 4 + c] = isCovered ? ubyte(MathUtil::Clamp(value * 255.0f + 0.5f, 0.0f, 255.0f)) : 0;
        }

        dst[i * 4 + 3] = isCovered ? 255 : 0;
    }

    return true;
}

AssetThumbnailService::AssetThumbnailService() = default;

AssetThumbnailService::~AssetThumbnailService()
{
    Shutdown();
}

void AssetThumbnailService::Initialize(World* world)
{
    AssertOnThread(g_simThread);

    m_world = world;
    m_resultSink = MakeShared<ResultSink>();

    m_previewScene = MakeUnique<AssetPreviewScene>(NAME("Thumbnail"), Vec2u(ThumbnailSize, ThumbnailSize));

    if (Handle<AssetRegistry> registry = GetCurrentAssetRegistry(); registry.IsValid())
    {
        m_onAssetMarkedDirty = registry->OnAssetMarkedDirty.Bind(
            [this](uint32 bucketIndex, Name assetName)
            {
                Invalidate(bucketIndex, assetName);
            });
    }
}

void AssetThumbnailService::Invalidate(uint32 bucketIndex, Name assetName)
{
    const PreviewAssetKey key { bucketIndex, assetName };

    if (!key.IsValid())
    {
        return;
    }

    Mutex::Guard guard(m_invalidationMutex);

    // Assets are marked dirty far more often than they are previewed, so the list is deduped here and
    // filtered down to ones we actually have a thumbnail for when it is drained.
    if (!m_pendingInvalidations.Contains(key))
    {
        m_pendingInvalidations.PushBack(key);
    }
}

void AssetThumbnailService::DrainInvalidations()
{
    Array<PreviewAssetKey> invalidations;

    {
        Mutex::Guard guard(m_invalidationMutex);

        if (m_pendingInvalidations.Empty())
        {
            return;
        }

        invalidations = std::move(m_pendingInvalidations);
        m_pendingInvalidations.Clear();
    }

    for (const PreviewAssetKey& key : invalidations)
    {
        // Only assets that have actually been thumbnailed are worth re-rendering. Without this, loading
        // or touching a project would queue a render for every asset in it and make them all resident.
        if (!GetThumbnailPath(key).Exists() && !m_undrawableKeys.Contains(key))
        {
            continue;
        }

        if (!m_forcedKeys.Contains(key))
        {
            m_forcedKeys.PushBack(key);
        }

        if (m_current != key && !m_queue.Contains(key))
        {
            m_queue.PushBack(key);
        }
    }
}

void AssetThumbnailService::Shutdown()
{
    m_onAssetMarkedDirty.Reset();

    {
        Mutex::Guard guard(m_invalidationMutex);
        m_pendingInvalidations.Clear();
    }

    m_forcedKeys.Clear();
    m_undrawableKeys.Clear();
    m_queue.Clear();
    m_current = {};
    m_stage = CaptureStage::Idle;
    m_framesInStage = 0;

    // Drop our reference; an in-flight readback keeps its own until the GPU retires the frame.
    m_resultSink.Reset();

    if (m_previewScene)
    {
        m_previewScene->Shutdown();
        m_previewScene.Reset();
    }

    m_world = nullptr;
}

FilePath AssetThumbnailService::GetThumbnailPath(const PreviewAssetKey& key) const
{
    const char* bucketName = GetAssetBucketName(key.bucketIndex);

    if (!bucketName)
    {
        return FilePath();
    }

    return EngineGlobals::GetCacheDirectory() / "Thumbnails"
        / bucketName
        / (String(*key.assetName) + ".png");
}

FilePath AssetThumbnailService::GetManifestPath(const PreviewAssetKey& key) const
{
    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();
    const char* bucketName = GetAssetBucketName(key.bucketIndex);

    if (!registry || !bucketName)
    {
        return FilePath();
    }

    return registry->GetRootPath()
        / bucketName
        / (String(*key.assetName) + ".hmf");
}

FilePath AssetThumbnailService::GetCachedThumbnailPath(uint32 bucketIndex, Name assetName) const
{
    const PreviewAssetKey key { bucketIndex, assetName };

    if (!key.IsValid())
    {
        return FilePath();
    }

    const FilePath thumbnailPath = GetThumbnailPath(key);

    if (!thumbnailPath.Exists())
    {
        return FilePath();
    }

    // A thumbnail older than the asset it depicts is stale - the asset has been edited since.
    const FilePath manifestPath = GetManifestPath(key);

    if (manifestPath.Exists() && manifestPath.LastModifiedTimestamp() > thumbnailPath.LastModifiedTimestamp())
    {
        return FilePath();
    }

    return thumbnailPath;
}

void AssetThumbnailService::Request(uint32 bucketIndex, Name assetName)
{
    AssertOnThread(g_simThread);

    const PreviewAssetKey key { bucketIndex, assetName };

    if (!key.IsValid())
    {
        return;
    }

    // A forced key's asset has changed since its thumbnail was rendered, so the cached file is stale even
    // though the mtime check cannot see it - the change may not have been saved to the manifest yet.
    if (!m_forcedKeys.Contains(key) && GetCachedThumbnailPath(bucketIndex, assetName).Any())
    {
        // Already up to date; tell the caller so it can pick the file up.
        OnThumbnailReady(bucketIndex, assetName);

        return;
    }

    if (m_current == key || m_queue.Contains(key))
    {
        return;
    }

    m_queue.PushBack(key);
}

void AssetThumbnailService::CancelPending()
{
    AssertOnThread(g_simThread);

    m_queue.Clear();
    m_undrawableKeys.Clear();
}

void AssetThumbnailService::Update()
{
    AssertOnThread(g_simThread);

    if (!m_world || !m_previewScene)
    {
        return;
    }

    DrainInvalidations();

    if (m_stage == CaptureStage::Idle)
    {
        BeginNextRequest();

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
                FinishCurrentRequest(false);

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
        ByteBuffer rgba8;

        FinishCurrentRequest(ConvertPreviewPixelsToRgba8(pixels, coverage, extent, rgba8)
            && WritePng(m_current, rgba8, extent));

        return;
    }

    if (m_framesInStage >= CaptureTimeoutFrames)
    {
        HYP_LOG(Editor, Warning, "Timed out capturing thumbnail for '{}'; skipping.", m_current.assetName);

        FinishCurrentRequest(false);
    }
}

void AssetThumbnailService::BeginNextRequest()
{
    if (m_queue.Empty())
    {
        return;
    }

    const PreviewAssetKey key = m_queue.PopFront();

    // Textures are already CPU-side; there is nothing to render for them.
    if (key.bucketIndex == AssetBuckets::Textures.GetIndex())
    {
        if (CaptureTextureDirectly(key))
        {
            OnThumbnailReady(key.bucketIndex, key.assetName);
        }

        return;
    }

    if (!PoseSubject(key))
    {
        if (!m_undrawableKeys.Contains(key))
        {
            m_undrawableKeys.PushBack(key);
        }

        return;
    }

    m_undrawableKeys.Erase(key);

    m_current = key;
    m_stage = CaptureStage::Settling;
    m_framesInStage = 0;
}

void AssetThumbnailService::FinishCurrentRequest(bool success)
{
    const PreviewAssetKey key = m_current;

    m_current = {};
    m_stage = CaptureStage::Idle;
    m_framesInStage = 0;

    m_forcedKeys.Erase(key);

    if (success)
    {
        OnThumbnailReady(key.bucketIndex, key.assetName);
    }
}

bool AssetThumbnailService::PoseSubject(const PreviewAssetKey& key)
{
    AssertOnThread(g_simThread);

    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry)
    {
        return false;
    }

    Handle<AssetObject> asset = registry->GetAsset(AssetBucket(key.bucketIndex), key.assetName);

    if (!asset.IsValid())
    {
        HYP_LOG(Editor, Warning, "Cannot render thumbnail: asset '{}' could not be resolved.", key.assetName);

        return false;
    }

    if (!m_previewScene->Initialize(m_world))
    {
        return false;
    }

    if (Material* material = DynamicCast<Material>(asset.Get()))
    {
        m_previewScene->ShowMaterial(material);

        return true;
    }

    if (Mesh* mesh = DynamicCast<Mesh>(asset.Get()))
    {
        m_previewScene->ShowMesh(mesh);

        return true;
    }

    if (Prefab* prefab = DynamicCast<Prefab>(asset.Get()))
    {
        return m_previewScene->ShowPrefab(prefab);
    }

    // Nothing meaningful to render for this asset type.
    return false;
}

bool AssetThumbnailService::CaptureTextureDirectly(const PreviewAssetKey& key)
{
    AssertOnThread(g_simThread);

    Handle<AssetRegistry> registry = GetCurrentAssetRegistry();

    if (!registry)
    {
        return false;
    }

    Handle<Texture> texture = registry->GetAsset<Texture>(AssetBucket(key.bucketIndex), key.assetName);

    if (!texture.IsValid())
    {
        return false;
    }

    const TextureDesc& desc = texture->GetTextureDesc();

    // Texture::Sample only decodes 8-bit-per-component formats, and only a 2D image has a sensible flat
    // preview. Anything else keeps its type icon rather than getting a black tile.
    if (desc.type != TextureType::Texture2D || TextureUtils::BytesPerComponent(desc.format) != 1)
    {
        return false;
    }

    if (desc.extent.x == 0 || desc.extent.y == 0 || texture->GetImageData().Size() == 0)
    {
        return false;
    }

    ByteBuffer rgba8(size_t(ThumbnailSize) * ThumbnailSize * 4);
    ubyte* dst = rgba8.Data();

    for (uint32 y = 0; y < ThumbnailSize; y++)
    {
        for (uint32 x = 0; x < ThumbnailSize; x++)
        {
            const Vec2f uv {
                (float(x) + 0.5f) / float(ThumbnailSize),
                (float(y) + 0.5f) / float(ThumbnailSize)
            };

            // Sample() already handles the source format and takes its own read scope.
            const Vec4f sample = texture->Sample2D(uv);

            const size_t offset = (size_t(y) * ThumbnailSize + x) * 4;

            for (uint32 c = 0; c < 4; c++)
            {
                dst[offset + c] = ubyte(MathUtil::Clamp(sample[c] * 255.0f + 0.5f, 0.0f, 255.0f));
            }
        }
    }

    return WritePng(key, rgba8, Vec2u(ThumbnailSize, ThumbnailSize));
}

bool AssetThumbnailService::WritePng(const PreviewAssetKey& key, const ByteBuffer& rgba8, Vec2u extent) const
{
    if (rgba8.Size() < size_t(extent.x) * extent.y * 4)
    {
        return false;
    }

    const FilePath thumbnailPath = GetThumbnailPath(key);

    if (thumbnailPath.Empty())
    {
        return false;
    }

    if (!thumbnailPath.BasePath().MkDir())
    {
        HYP_LOG(Editor, Warning, "Failed to create thumbnail cache directory '{}'.", thumbnailPath.BasePath());

        return false;
    }

    const int result = stbi_write_png(
        thumbnailPath.Data(),
        int(extent.x),
        int(extent.y),
        4,
        rgba8.Data(),
        int(extent.x) * 4);

    if (result == 0)
    {
        HYP_LOG(Editor, Warning, "Failed to write thumbnail '{}'.", thumbnailPath);

        return false;
    }

    return true;
}

} // namespace Hyperion

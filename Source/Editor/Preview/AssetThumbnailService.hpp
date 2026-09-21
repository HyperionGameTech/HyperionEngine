/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/SharedPtr.hpp>
#include <Core/Memory/UniquePtr.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Editor/Preview/AssetPreviewScene.hpp>

namespace Hyperion {

class World;

struct PreviewAssetKey
{
    uint32 bucketIndex = 0;
    Name assetName;

    HYP_FORCE_INLINE bool operator==(const PreviewAssetKey& other) const
    {
        return bucketIndex == other.bucketIndex && assetName == other.assetName;
    }

    HYP_FORCE_INLINE bool operator!=(const PreviewAssetKey& other) const
    {
        return !(*this == other);
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return bucketIndex != 0 && assetName.IsValid();
    }
};

bool ConvertPreviewPixelsToRgba8(
    const ByteBuffer& linearRgba16f,
    const ByteBuffer& coverageRgba16f,
    Vec2u extent,
    ByteBuffer& outRgba8);

class AssetThumbnailService final
{
public:
    static constexpr uint32 ThumbnailSize = 128;

    AssetThumbnailService();
    ~AssetThumbnailService();

    AssetThumbnailService(const AssetThumbnailService& other) = delete;
    AssetThumbnailService& operator=(const AssetThumbnailService& other) = delete;

    void Initialize(World* world);
    void Shutdown();

    FilePath GetCachedThumbnailPath(uint32 bucketIndex, Name assetName) const;

    /*! \brief Queue a thumbnail render. Does nothing when a current thumbnail is already cached, or when
     *  the asset is already queued or in flight. */
    void Request(uint32 bucketIndex, Name assetName);

    /*! \brief Drop every queued request that has not started yet. Used when the user switches bucket so
     *  the queue does not keep working on assets that are no longer on screen. */
    void CancelPending();

    /*! \brief Re-render an asset's thumbnail even though one is already cached. Called from the registry's
     *  dirty signal, so it may run on any thread; the work itself is deferred to the next Update(). */
    void Invalidate(uint32 bucketIndex, Name assetName);

    void Update();

    Delegate<void, uint32, Name> OnThumbnailReady;

private:
    enum class CaptureStage : uint8
    {
        Idle,
        Settling, //!< posed, waiting for collection and GPU uploads before capturing
        Capturing //!< capture armed, waiting for the renderer and then the readback
    };

    void DrainInvalidations();

    bool PoseSubject(const PreviewAssetKey& key);
    bool CaptureTextureDirectly(const PreviewAssetKey& key);

    void BeginNextRequest();
    void FinishCurrentRequest(bool success);

    bool WritePng(const PreviewAssetKey& key, const ByteBuffer& rgba8, Vec2u extent) const;

    FilePath GetThumbnailPath(const PreviewAssetKey& key) const;
    FilePath GetManifestPath(const PreviewAssetKey& key) const;

    World* m_world = nullptr;

    UniquePtr<AssetPreviewScene> m_previewScene;

    Array<PreviewAssetKey> m_queue;

    //!< Assets whose cached thumbnail is known to be out of date, so Request() ignores the cache for them.
    Array<PreviewAssetKey> m_forcedKeys;

    //!< Requested but had nothing to render
    Array<PreviewAssetKey> m_undrawableKeys;

    //!< Written from whichever thread changed an asset, drained on the sim thread in Update().
    Mutex m_invalidationMutex;
    Array<PreviewAssetKey> m_pendingInvalidations;

    DelegateHandler m_onAssetMarkedDirty;

    PreviewAssetKey m_current;
    CaptureStage m_stage = CaptureStage::Idle;
    uint32 m_framesInStage = 0;

    struct ResultSink
    {
        Mutex mutex;
        ByteBuffer pixels;
        ByteBuffer coverage;
        Vec2u extent;
        bool hasResult = false;
    };

    SharedPtr<ResultSink> m_resultSink;
};

} // namespace Hyperion

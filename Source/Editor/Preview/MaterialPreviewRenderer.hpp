/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Functional/Delegate.hpp>

#include <Core/Math/Vector2.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/SharedPtr.hpp>
#include <Core/Memory/UniquePtr.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Editor/Preview/AssetPreviewScene.hpp>

namespace Hyperion {

class World;
class Material;

class MaterialPreviewRenderer final
{
public:
    static constexpr uint32 PreviewSize = 256;

    MaterialPreviewRenderer();
    ~MaterialPreviewRenderer();

    MaterialPreviewRenderer(const MaterialPreviewRenderer& other) = delete;
    MaterialPreviewRenderer& operator=(const MaterialPreviewRenderer& other) = delete;

    void Initialize(World* world);
    void Shutdown();

    void SetMaterial(uint32 bucketIndex, Name assetName);

    HYP_FORCE_INLINE bool IsActive() const
    {
        return m_isActive;
    }

    void SetLightAngles(float yaw, float pitch);

    void Invalidate();
    void Update();
    
    size_t CopyLatestFrame(void* dest, size_t destSize, uint32& outWidth, uint32& outHeight) const;

    Delegate<void> OnFrameReady;

private:
    enum class CaptureStage : uint8
    {
        Idle,
        Settling,
        Capturing
    };

    bool BeginRender();
    void ApplyLightAngles();

    World* m_world = nullptr;

    UniquePtr<AssetPreviewScene> m_previewScene;

    uint32 m_bucketIndex = 0;
    Name m_assetName;
    bool m_isActive = false;

    AtomicVar<bool> m_isDirty { false };

    float m_lightYaw = 0.0f;
    float m_lightPitch = 0.0f;

    CaptureStage m_stage = CaptureStage::Idle;
    uint32 m_framesInStage = 0;

    //!< Latest converted frame, written on the sim thread and read by the editor UI thread.
    mutable Mutex m_frameMutex;
    ByteBuffer m_latestFrame;
    Vec2u m_latestExtent;

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

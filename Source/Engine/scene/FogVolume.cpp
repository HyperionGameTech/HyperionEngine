/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/FogVolume.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Shared.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/threading/Threads.hpp>

#include <Core/math/Vector3.hpp>
#include <Core/math/MathUtil.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#if HYP_EDITOR
#include <baking/BakerSubsystem.hpp>
#include <baking/fog_volume/FogVolumeBakeData.hpp>
#endif

#include <FogVolume.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
HYP_DECLARE_LOG_CHANNEL(Editor);
#endif

FogVolume::FogVolume()
{
}

FogVolume::FogVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

FogVolume::~FogVolume()
{
    if (m_volumeTexture)
    {
        EnqueueDeletion(std::move(m_volumeTexture));
    }

    if (m_noiseTexture)
    {
        EnqueueDeletion(std::move(m_noiseTexture));
    }
}

void FogVolume::Init()
{
    VolumeBase::Init();

    if (m_volumeTexture)
    {
        CheckResult(m_volumeTexture->Create());
    }

    if (m_noiseTexture)
    {
        CheckResult(m_noiseTexture->Create());
    }

    SetNeedsRenderProxyUpdate();

    SetReady(true);
}

void FogVolume::SetTextures(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture)
{
    if (m_volumeTexture != volumeTexture)
    {
        if (m_volumeTexture)
        {
            EnqueueDeletion(std::move(m_volumeTexture));
        }

        m_volumeTexture = volumeTexture;
    }

    if (m_noiseTexture != noiseTexture)
    {
        if (m_noiseTexture)
        {
            EnqueueDeletion(std::move(m_noiseTexture));
        }

        m_noiseTexture = noiseTexture;
    }

    if (IsInitCalled())
    {
        if (m_volumeTexture.IsValid())
        {
            CheckResult(m_volumeTexture->Create());
        }

        SetNeedsRenderProxyUpdate();
    }
}

void FogVolume::UpdateRenderProxy(RenderProxyFogVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    const BoundingBox worldAabb = GetWorldBounds();

    proxy->fogVolume = WeakHandleFromThis();
    proxy->worldAabb = worldAabb;

    if (proxy->volumeTexture != m_volumeTexture)
    {
        proxy->forceRebind = true;

        proxy->volumeTexture = m_volumeTexture;
    }

    if (proxy->noiseTexture != m_noiseTexture)
    {
        proxy->forceRebind = true;

        proxy->noiseTexture = m_noiseTexture;
    }

    // create transform matrix turning 1:1:1 cube to the world bounds
    const Vec3f boxSize = m_localBounds.GetExtent() * 0.5f;

    const Mat4f newTransformMatrix = GetWorldMatrix() * Mat4f::Scaling(boxSize);
    if (newTransformMatrix != proxy->bufferData.transformMatrix
        || worldAabb.min != proxy->bufferData.aabbMin.GetXYZ()
        || worldAabb.max != proxy->bufferData.aabbMax.GetXYZ())
    {
        proxy->forceRebind = true;

        proxy->bufferData.transformMatrix = newTransformMatrix;
        proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
        proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    }
}

#if HYP_EDITOR

void FogVolume::Rebake()
{
    HYP_SCOPE;

    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(MakeStrongRef(this));
}

#endif

} // namespace Hyperion

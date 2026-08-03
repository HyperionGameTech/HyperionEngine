/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/FogVolume.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Shared.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#ifdef HYP_EDITOR
#include <Baking/BakerSubsystem.hpp>
#include <Baking/FogVolume/FogVolumeBakeData.hpp>
#endif

#include <FogVolume.generated.inl>

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

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
        Check(m_volumeTexture->Create());
    }

    if (m_noiseTexture)
    {
        Check(m_noiseTexture->Create());
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
            Check(m_volumeTexture->Create());
        }

        SetNeedsRenderProxyUpdate();
    }
}

void FogVolume::UpdateRenderProxy(RenderProxyFogVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    const BoundingBox worldAabb = GetWorldBounds();

    proxy->fogVolume = this;
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
    const Mat4f newTransformMatrix = GetWorldMatrix()
        * Mat4f::Translation(m_localBounds.GetCenter())
        * Mat4f::Scaling(m_localBounds.GetExtent() * 0.5f);

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

#ifdef HYP_EDITOR

void FogVolume::Rebake()
{
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

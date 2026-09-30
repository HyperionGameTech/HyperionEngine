/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/FogVolume.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

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

namespace {

SwatchOverrideSystem* GetSwatchOverrideSystem(const FogVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<SwatchOverrideSystem>() : nullptr;
}

} // namespace

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

void FogVolume::SetVolumeTexture(const Handle<Texture>& volumeTexture)
{
    if (m_volumeTexture == volumeTexture)
    {
        return;
    }

    if (m_volumeTexture)
    {
        EnqueueDeletion(std::move(m_volumeTexture));
    }

    m_volumeTexture = volumeTexture;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetNoiseTexture(const Handle<Texture>& noiseTexture)
{
    if (m_noiseTexture == noiseTexture)
    {
        return;
    }

    if (m_noiseTexture)
    {
        EnqueueDeletion(std::move(m_noiseTexture));
    }

    m_noiseTexture = noiseTexture;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetTextures(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture)
{
    SetVolumeTexture(volumeTexture);
    SetNoiseTexture(noiseTexture);
}

Name FogVolume::BuildVolumeTextureName(Name volumeName, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return NAME_FMT("FogVolume_{}_DataMap", volumeName);
    }

    return NAME_FMT("FogVolume_{}_{}_DataMap", volumeName, swatchName);
}

Name FogVolume::BuildNoiseTextureName(Name volumeName, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return NAME_FMT("FogVolume_{}_NoiseMap", volumeName);
    }

    return NAME_FMT("FogVolume_{}_{}_NoiseMap", volumeName, swatchName);
}

Handle<Texture> FogVolume::GetVolumeTextureForSwatch(Name swatchName) const
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return GetVolumeTexture();
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return GetVolumeTexture();
    }

    BoxedValue overrideValue;

    if (!swatchOverrideSystem->GetSwatchOverrideValue(this, swatchName, GetVolumeTexturePropertyName(), overrideValue))
    {
        return GetVolumeTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Swatch override '{}' on FogVolume '{}' is not a texture",
        swatchName, GetName());

    return GetVolumeTexture();
}

Handle<Texture> FogVolume::GetNoiseTextureForSwatch(Name swatchName) const
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return GetNoiseTexture();
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return GetNoiseTexture();
    }

    BoxedValue overrideValue;

    if (!swatchOverrideSystem->GetSwatchOverrideValue(this, swatchName, GetNoiseTexturePropertyName(), overrideValue))
    {
        return GetNoiseTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Swatch override '{}' on FogVolume '{}' is not a texture",
        swatchName, GetName());

    return GetNoiseTexture();
}

void FogVolume::SetTexturesForSwatch(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture,
    Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        SetTextures(volumeTexture, noiseTexture);

        return;
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign textures for swatch '{}' on FogVolume '{}': no SwatchOverrideSystem",
            swatchName, GetName());

        return;
    }

    if (GetVolumeTextureForSwatch(swatchName) == volumeTexture
        && GetNoiseTextureForSwatch(swatchName) == noiseTexture)
    {
        return;
    }

    if (volumeTexture.IsValid())
    {
        volumeTexture->SetName(BuildVolumeTextureName(GetName(), swatchName));

        if (!volumeTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(volumeTexture);
        }
    }

    if (noiseTexture.IsValid())
    {
        noiseTexture->SetName(BuildNoiseTextureName(GetName(), swatchName));

        if (!noiseTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(noiseTexture);
        }
    }

    swatchOverrideSystem->AddSwatchOverrideSet(this, swatchName);
    swatchOverrideSystem->SetSwatchOverrideValue(this, swatchName, GetVolumeTexturePropertyName(), BoxedValue(volumeTexture));
    swatchOverrideSystem->SetSwatchOverrideValue(this, swatchName, GetNoiseTexturePropertyName(), BoxedValue(noiseTexture));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

#ifdef HYP_EDITOR

Array<Name> FogVolume::GetBakedSwatchNames() const
{
    Array<Name> swatchNames;

    if (m_volumeTexture.IsValid())
    {
        swatchNames.PushBack(g_defaultSwatchName);
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return swatchNames;
    }

    const Name propertyName = GetVolumeTexturePropertyName();

    for (Name swatchName : swatchOverrideSystem->GetSetSwatchNames(this))
    {
        if (swatchOverrideSystem->IsPropertyOverriddenInSwatch(this, swatchName, propertyName))
        {
            swatchNames.PushBack(swatchName);
        }
    }

    return swatchNames;
}

#endif // HYP_EDITOR

void FogVolume::SetDensity(float density)
{
    if (m_density == density)
    {
        return;
    }

    m_density = density;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetAlbedo(const Vec3f& albedo)
{
    if (m_albedo == albedo)
    {
        return;
    }

    m_albedo = albedo;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetPhaseForward(float phaseForward)
{
    if (m_phaseForward == phaseForward)
    {
        return;
    }

    m_phaseForward = phaseForward;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetPhaseBackward(float phaseBackward)
{
    if (m_phaseBackward == phaseBackward)
    {
        return;
    }

    m_phaseBackward = phaseBackward;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetPhaseBlend(float phaseBlend)
{
    if (m_phaseBlend == phaseBlend)
    {
        return;
    }

    m_phaseBlend = phaseBlend;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetSunIntensity(float sunIntensity)
{
    if (m_sunIntensity == sunIntensity)
    {
        return;
    }

    m_sunIntensity = sunIntensity;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetAmbientIntensity(float ambientIntensity)
{
    if (m_ambientIntensity == ambientIntensity)
    {
        return;
    }

    m_ambientIntensity = ambientIntensity;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetEdgeFade(float edgeFade)
{
    if (m_edgeFade == edgeFade)
    {
        return;
    }

    m_edgeFade = edgeFade;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
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

    const Vec4f medium = Vec4f(MathUtil::Max(m_density, 0.0f), MathUtil::Clamp(m_phaseForward, -0.95f, 0.95f), MathUtil::Clamp(m_phaseBackward, -0.95f, 0.95f), MathUtil::Clamp(m_phaseBlend, 0.0f, 1.0f));
    const Vec4f lighting = Vec4f(Vec3f::Max(Vec3f::Min(m_albedo, Vec3f(1.0f)), Vec3f(0.0f)), MathUtil::Max(m_ambientIntensity, 0.0f));
    const Vec4f shape = Vec4f(MathUtil::Max(m_sunIntensity, 0.0f), MathUtil::Max(m_edgeFade, 0.0f), 0.0f, 0.0f);

    if (medium != proxy->bufferData.medium || lighting != proxy->bufferData.lighting || shape != proxy->bufferData.shape)
    {
        proxy->forceRebind = true;

        proxy->bufferData.medium = medium;
        proxy->bufferData.lighting = lighting;
        proxy->bufferData.shape = shape;
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

    const Handle<Swatch>& swatch = world->GetActiveSwatch();

    if (!swatch.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: could not resolve the active swatch", GetName());

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(swatch->bakeLayer, MakeStrongRef(this));
}

#endif

} // namespace Hyperion

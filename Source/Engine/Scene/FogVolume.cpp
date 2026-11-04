/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/FogVolume.hpp>

#include <Rendering/RenderProxy.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/MathUtil.hpp>

#include <FogVolume.generated.inl>

namespace Hyperion {

FogVolume::FogVolume()
{
}

FogVolume::FogVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

FogVolume::~FogVolume() = default;

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

    const Vec4f medium = Vec4f(MathUtil::Max(m_density, 0.0f), MathUtil::Clamp(m_phaseForward, -0.95f, 0.95f), MathUtil::Clamp(m_phaseBackward, -0.95f, 0.95f), MathUtil::Clamp(m_phaseBlend, 0.0f, 1.0f));
    const Vec4f lighting = Vec4f(Vec3f::Max(Vec3f::Min(m_albedo, Vec3f(1.0f)), Vec3f(0.0f)), MathUtil::Max(m_ambientIntensity, 0.0f));
    const Vec4f shape = Vec4f(MathUtil::Max(m_sunIntensity, 0.0f), MathUtil::Max(m_edgeFade, 0.0f), 0.0f, 0.0f);

    if (worldAabb != proxy->worldAabb
        || medium != proxy->bufferData.medium
        || lighting != proxy->bufferData.lighting
        || shape != proxy->bufferData.shape)
    {
        proxy->forceRebind = true;

        proxy->worldAabb = worldAabb;

        proxy->bufferData.medium = medium;
        proxy->bufferData.lighting = lighting;
        proxy->bufferData.shape = shape;
    }
}

} // namespace Hyperion

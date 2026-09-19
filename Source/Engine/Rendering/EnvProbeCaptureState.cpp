/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/EnvProbeCaptureState.hpp>

#include <Scene/EnvProbe.hpp>

#include <Scene/Swatch.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

#include <Scene/World.hpp>

#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>

namespace Hyperion {

EnvProbeCaptureState::EnvProbeCaptureState(EnvProbe* envProbe, Name swatchName)
    : swatchName(swatchName),
      m_envProbe(envProbe)
{
}

EnvProbeCaptureState::~EnvProbeCaptureState()
{
    if (!m_envProbe)
    {
        return;
    }

    if (m_envProbe->GetCaptureState() == this)
    {
        m_envProbe->m_captureState = nullptr;
        m_envProbe->SetNeedsRenderProxyUpdate();
    }
}

void EnvProbeCaptureState::Begin()
{
    Assert(m_envProbe != nullptr);

    if (!m_envProbe)
    {
        return;
    }

    m_envProbe->SetDimensions(EnvProbe::GetDefaultDimensions(m_envProbe->GetEnvProbeType()));

    m_envProbe->m_captureState = this;

    const int32 numReadbacks = (m_envProbe->ShouldComputeSphericalHarmonics() ? 1 : 0)
        + ((m_envProbe->GetEnvProbeFlags() & EPF_VISIBILITY) ? 1 : 0)
        + ((m_envProbe->GetEnvProbeFlags() & EPF_HIT_MASK) ? 1 : 0);

    m_envProbe->m_pendingCaptureReadbacks.Set(numReadbacks, MemoryOrder::RELEASE);

    m_envProbe->InitCaptureData(this);

    // InitCaptureData(captureState) doesn't mark the proxy dirty itself (SetDimensions() above
    // only does so if the dimensions actually changed), so force a sync here - the render thread
    // must pick up this capture's texture(s) via the proxy rather than reading captureState directly.
    m_envProbe->SetNeedsRenderProxyUpdate();

    m_envProbe->needsRender.Store(true);
}

void EnvProbeCaptureState::End(bool commitResult)
{
    Assert(m_envProbe != nullptr);

    if (!m_envProbe)
    {
        return;
    }

    if (m_envProbe->GetCaptureState() == this)
    {
        m_envProbe->m_captureState = nullptr;
        m_envProbe->SetNeedsRenderProxyUpdate();
    }

    if (!commitResult)
    {
        // abandon ship
        return;
    }

    if (texture.IsValid())
    {
        texture->SetIsTransient(false);
        texture->SetName(EnvProbe::BuildBakedTextureName(m_envProbe->GetName(), swatchName));

        GetCurrentAssetRegistry()->PutAssetUnique(texture);

        m_envProbe->SetBakedTextureForSwatch(texture, swatchName);
    }

    if (visibilityTexture.IsValid())
    {
        visibilityTexture->SetIsTransient(false);
        visibilityTexture->SetName(EnvProbe::BuildVisibilityTextureName(m_envProbe->GetName(), swatchName));

        GetCurrentAssetRegistry()->PutAssetUnique(visibilityTexture);

        m_envProbe->SetVisibilityTextureForSwatch(visibilityTexture, swatchName);
    }

    if (m_envProbe->ShouldComputeSphericalHarmonics())
    {
        m_envProbe->SetSphericalHarmonicsDataForSwatch(sphericalHarmonics, swatchName);
    }

    m_envProbe->DestroyCaptureData();

    // Set the overrides!
    if (World* world = m_envProbe->GetWorld(); world && world->GetActiveSwatchName() == swatchName)
    {
        if (SwatchOverrideSystem* swatchOverrideSystem = world->GetSystem<SwatchOverrideSystem>())
        {
            swatchOverrideSystem->ApplyOverrides(m_envProbe, swatchName);
        }
    }
}

} // namespace Hyperion

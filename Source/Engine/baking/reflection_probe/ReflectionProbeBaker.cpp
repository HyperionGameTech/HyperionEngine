/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <baking/reflection_probe/ReflectionProbeBakeJob.hpp>

#include <rendering/Texture.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <scene/EnvProbe.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {

namespace ConvolveProbe {
void ConvolveEnvProbeCubemap(
    const Handle<Texture>& inTexture,
    const EnvProbe& envProbe);
} // namespace ConvolveProbe

namespace ComputeSH {
void ComputeEnvProbeSphericalHarmonics(
    const EnvProbe& envProbe,
    const Texture& inColorTexture);
} // namespace ComputeSH

namespace Baking {

Baker<ReflectionProbe>::Baker(BakerConfig&& config, const Handle<ReflectionProbe>& envProbe)
    : BakerBase(std::move(config), envProbe, MakeStrongRef(envProbe->GetScene()), BoundingBox::Empty()),
      m_envProbe(envProbe)
{
}

UniquePtr<BakeJobBase> Baker<ReflectionProbe>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<ReflectionProbe>>(std::move(params), m_envProbe, &m_bakeData);
}

void Baker<ReflectionProbe>::CreateLightmapRenderers()
{
    m_lightmapRenderers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
    AssertDebug(maxTexelsPerFrame > 0);

    UniquePtr<ILightmapRenderer>& lightmapRenderer = m_lightmapRenderers.EmplaceBack();
    lightmapRenderer = CreateRenderer(LightmapShadingType::FULL, maxTexelsPerFrame);

    if (!lightmapRenderer)
    {
        m_lightmapRenderers.PopBack();
        return;
    }

    lightmapRenderer->Create();
}

Result Baker<ReflectionProbe>::Build_Internal()
{
    Assert(m_envProbe != nullptr);

    InitObject(m_envProbe);
    m_bakeData = BakeData<ReflectionProbe>(m_bakeEntities, m_envProbe.Get());

    return m_bakeData.Build();
}

void Baker<ReflectionProbe>::OnCompleted_Internal()
{
    HYP_SCOPE;

    AssertDebug(m_bakeData.IsBuilt());
    if (!m_bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for EnvProbe {} is not built, skipping texture creation", m_envProbe->Id());
        return;
    }

    const Vec2u dimensions = m_envProbe->GetDimensions();

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    BakeData<ReflectionProbe>::BitmapType bitmap = m_bakeData.ToBitmap();

    TextureDesc textureDesc {
        TextureType::Cubemap,
        bitmap.GetFormat(),
        Vec3u { dimensions.x, dimensions.y, 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    Handle<Texture> cubemap = MakeHandle<Texture>(textureDesc, bitmap.ToByteView());
    cubemap->SetName(NAME_FMT("{}_SrcCubemap", m_envProbe->GetName()));
    cubemap->SetIsTransient(true);

    // Covolves the env probe cubemap and computes SH coefficients on the GPU
    struct ProcessEnvProbeCommand : public RenderCommand
    {
        Handle<EnvProbe> envProbe;
        Handle<Texture> cubemap;

        ProcessEnvProbeCommand(const Handle<EnvProbe>& envProbe, const Handle<Texture>& cubemap)
            : envProbe(envProbe),
              cubemap(cubemap)
        {
        }

        ~ProcessEnvProbeCommand() override
        {
        }

        RendererResult operator()() override
        {
            CheckResult(cubemap->Create());

            // prevent writing on other threads
            auto resGuard = envProbe->GetWriteScope();

            Handle<Texture> prefiltered = MakeHandle<Texture>(TextureDesc {
                TextureType::Cubemap,
                cubemap->GetFormat(),
                Vec3u { envProbe->GetDimensions().x, envProbe->GetDimensions().y, 1 },
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            });

            prefiltered->SetName(NAME_FMT("{}_Prefiltered", envProbe->GetName()));
            GetCurrentAssetRegistry()->PutAsset(prefiltered);

            CheckResult(prefiltered->Create());

            envProbe->SetBakedTexture(prefiltered);

            ConvolveProbe::ConvolveEnvProbeCubemap(cubemap, *envProbe);
            ComputeSH::ComputeEnvProbeSphericalHarmonics(*envProbe, *cubemap);

            RI.GetCurrentFrame()->OnFrameEnd.Bind([cubemap = cubemap, envProbe = envProbe](Frame*) mutable
                {
                    EnqueueDeletion(std::move(cubemap));
                    EnqueueDeletion(std::move(envProbe));
                });

            return {};
        }
    };

    PUSH_RENDER_COMMAND(ProcessEnvProbeCommand, m_envProbe, cubemap);

    HYP_LOG(Lightmap, Verbose, "EnvProbe {} lightmap baking complete! Radiance and irradiance textures created.", m_envProbe->Id());
}

} // namespace Baking
} // namespace Hyperion

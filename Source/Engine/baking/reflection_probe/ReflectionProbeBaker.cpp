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

#include <rendering/passes/EnvProbePass.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <scene/EnvProbe.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

extern ENGINE_API const FilePath& GetTempDirectory();

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

    // prevent writing on other threads
    auto resGuard = m_envProbe->GetWriteScope();

    const Vec2u dimensions = m_envProbe->GetDimensions();
    AssertDebug(dimensions.Volume() > 0);

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    BakeData<ReflectionProbe>::BitmapType bitmap = m_bakeData.ToBitmap();

    TextureDesc desc {
        TextureType::Cubemap,
        bitmap.GetFormat(),
        Vec3u { dimensions, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    ByteBuffer buffer = ByteBuffer(bitmap.ToByteView());

    //// temp
    FileByteWriter tempWriter(GetTempDirectory() / "TempEnvProbe.bmp");
    bitmap.Write(&tempWriter);
    tempWriter.Close();

    bitmap = {};

    Texture::GenerateMipmaps(desc, buffer);

    Handle<Texture> prefiltered = MakeHandle<Texture>(desc, buffer.ToByteView());

    buffer.Clear();

    prefiltered->SetName(NAME_FMT("{}_Prefiltered", m_envProbe->GetName()));
    GetCurrentAssetRegistry()->PutAsset(prefiltered);

    CheckResult(prefiltered->Create());

    m_envProbe->SetBakedTexture(prefiltered);

    // Covolves the env probe cubemap and computes SH coefficients on the GPU
    struct ProcessReflectionProbePayload
    {
        Handle<EnvProbe> envProbe;
    };

    class ProcessReflectionProbe : public CmdBase
    {
    public:
        ProcessReflectionProbePayload* payload;

        explicit ProcessReflectionProbe(ProcessReflectionProbePayload* payload)
            : payload(payload)
        {

        }

        static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
        {
            ProcessReflectionProbe* cmdCasted = static_cast<ProcessReflectionProbe*>(cmd);

            const Handle<EnvProbe>& envProbe = cmdCasted->payload->envProbe;

            auto resGuard = envProbe->GetWriteScope();

            const Handle<Texture>& texture = envProbe->GetBakedTexture();

            Assert(texture.IsValid());

            ConvolveProbe::ConvolveEnvProbeCubemap(texture, *envProbe);
            ComputeSH::ComputeEnvProbeSphericalHarmonics(*envProbe, *texture);

            HYP_LOG(Lightmap, Verbose, "EnvProbe {} lightmap baking complete! Radiance and irradiance textures created.", envProbe->Id());

            delete cmdCasted->payload;
        }
    };

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    cr << ProcessReflectionProbe(new ProcessReflectionProbePayload { m_envProbe });
    cr.Done();
}

} // namespace Baking
} // namespace Hyperion

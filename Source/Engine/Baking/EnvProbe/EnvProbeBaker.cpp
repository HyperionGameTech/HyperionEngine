/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/Lightmaps/LightmapPathTraceGpu.hpp>

#include <Baking/EnvProbe/EnvProbeBaker.hpp>
#include <Baking/EnvProbe/EnvProbeBakeJob.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/ProbeVolume.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

extern ENGINE_API const FilePath& GetTempDirectory();

namespace EnvProbeHelpers {

void ConvolveEnvProbeCubemap(
    const Handle<Texture>& inTexture,
    const EnvProbe& envProbe);

void ComputeEnvProbeSphericalHarmonics(
    const EnvProbe& envProbe,
    const Texture& inColorTexture);

} // namespace EnvProbeHelpers

namespace Baking {

Baker<EnvProbe>::Baker(BakerConfig&& config, const Handle<EnvProbe>& envProbe)
    : BakerBase(std::move(config), envProbe, MakeStrongRef(envProbe->GetScene()), BoundingBox::Empty()),
      m_envProbe(envProbe)
{
}

UniquePtr<BakeJobBase> Baker<EnvProbe>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<EnvProbe>>(std::move(params), m_envProbe, &m_bakeData);
}

void Baker<EnvProbe>::CreateLightmapRenderers()
{
    m_pathTracers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 shadingTypesMask = GetShadingTypesMask();
    const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
    AssertDebug(maxTexelsPerFrame > 0);

    for (uint32 i = 0; i < uint32(LightmapShadingType::MAX); i++)
    {
        if (!(shadingTypesMask & (1u << i)))
        {
            continue;
        }

        const UniquePtr<PathTracer>& pathTracer = m_pathTracers.PushBack(CreatePathTracer(LightmapShadingType(i), maxTexelsPerFrame));

        if (!pathTracer)
        {
            m_pathTracers.PopBack();
            
            continue;
        }

        pathTracer->Create();
    }
}

Result Baker<EnvProbe>::Build_Internal()
{
    Assert(m_envProbe != nullptr);

    InitObject(m_envProbe);
    m_bakeData = BakeData<EnvProbe>(m_bakeEntities, m_envProbe.Get());

    return m_bakeData.Build();
}

void Baker<EnvProbe>::OnCompleted_Internal()
{
    HYP_SCOPE;

    AssertDebug(m_bakeData.IsBuilt());
    if (!m_bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for EnvProbe {} is not built, skipping texture creation", m_envProbe->Id());
        return;
    }

    // prevent writing on other threads
    auto envProbeWriteScope = TUniqueResLock<EnvProbe>(*m_envProbe);

    const Vec2u dimensions = m_envProbe->GetDimensions();
    AssertDebug(dimensions.Volume() > 0);

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    BakeData<EnvProbe>::BitmapType bitmap = m_bakeData.ToBitmap();

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
    FileByteWriter tempWriter(EngineGlobals::GetTempDirectory() / "TempEnvProbe.bmp");
    bitmap.Write(&tempWriter);
    tempWriter.Close();

    bitmap = {};

    Texture::GenerateMipmaps(desc, buffer);

    Handle<Texture> bakedTexture = MakeHandle<Texture>(desc, buffer.ToByteView());

    buffer.Clear();

    bakedTexture->SetName(NAME_FMT("{}_ColorMap", m_envProbe->GetName()));

    // Ambient probes don't save their texture; it is transient,
    // only used for calc'ing SH
    if (m_envProbe->IsAmbientProbe())
    {
        bakedTexture->SetIsTransient(true);
    }

    GetCurrentAssetRegistry()->PutAssetUnique(bakedTexture);

    m_envProbe->SetBakedTexture(bakedTexture);

    // Bake visibility texture
    if (m_envProbe->GetEnvProbeFlags() & EPF_VISIBILITY)
    {
        BakeData<EnvProbe>::VisibilityBitmapType visBitmap = m_bakeData.ToVisibilityBitmap();

        // Visibility texture dimensions must match the global envProbesDepthTexture (16x16).
        static constexpr uint32 visDim = 16;

        TextureDesc visDesc {
            TextureType::Cubemap,
            visBitmap.GetFormat(),
            Vec3u { visDim, visDim, 1 },
            TFM_LINEAR,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED | IU_STORAGE
        };

        ByteBuffer visBuffer = ByteBuffer(visBitmap.ToByteView());

        visBitmap = {};

        Handle<Texture> visibilityTexture = MakeHandle<Texture>(visDesc, visBuffer.ToByteView());

        visBuffer.Clear();

        visibilityTexture->SetName(NAME_FMT("{}_VisibilityMap", m_envProbe->GetName()));

        // SetVisibilityTexture handles Create() and asset registration.
        m_envProbe->SetVisibilityTexture(visibilityTexture);
    }

    // Convolves the env probe cubemap and computes SH coefficients on the GPU
    struct ProcessEnvProbePayload
    {
        Handle<EnvProbe> envProbe;
    };

    class ProcessEnvProbe : public CmdBase
    {
    public:
        ProcessEnvProbePayload* payload;

        explicit ProcessEnvProbe(ProcessEnvProbePayload* payload)
            : payload(payload)
        {
        }

        static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
        {
            ProcessEnvProbe* cmdCasted = static_cast<ProcessEnvProbe*>(cmd);

            const Handle<EnvProbe>& envProbe = cmdCasted->payload->envProbe;

            auto envProbeWriteScope = TUniqueResLock<EnvProbe>(*envProbe);

            const Handle<Texture>& texture = envProbe->GetBakedTexture();

            Assert(texture.IsValid());

            if (envProbe->ShouldComputePrefilteredEnvMap())
            {
                EnvProbeHelpers::ConvolveEnvProbeCubemap(texture, *envProbe);
            }

            if (envProbe->ShouldComputeSphericalHarmonics())
            {
                EnvProbeHelpers::ComputeEnvProbeSphericalHarmonics(*envProbe, *texture);
            }

            HYP_LOG(Lightmap, Verbose, "EnvProbe {} lightmap baking complete", envProbe->GetName());

            delete cmdCasted->payload;
        }
    };

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    cr << ProcessEnvProbe(new ProcessEnvProbePayload { m_envProbe });
    cr.Done();

    // For irradiance probes, notify the parent ProbeVolume so that affected
    // entities are tagged for SH re-evaluation.
    m_envProbe->Invalidate(true);
}

} // namespace Baking
} // namespace Hyperion

/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/FogVolume/FogVolumeBaker.hpp>
#include <Baking/FogVolume/FogVolumeBakeJob.hpp>

#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Scene/FogVolume.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

Baker<FogVolume>::Baker(BakerConfig&& config, BakeLayer& bakeLayer, const Handle<FogVolume>& fogVolume)
    : BakerBase(std::move(config), bakeLayer, fogVolume, MakeStrongRef(fogVolume->GetScene()), fogVolume->GetWorldBounds()),
      m_fogVolume(fogVolume)
{
}

UniquePtr<BakeJobBase> Baker<FogVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<FogVolume>>(std::move(params), m_fogVolume, &m_bakeData);
}

void Baker<FogVolume>::Build()
{
    HYP_SCOPE;

    Assert(m_fogVolume != nullptr);
    Assert(m_numJobs == 0, "Cannot initialize fog volume baker -- jobs currently running!");

    GatherBakeEntities();

    m_bakeData = BakeData<FogVolume>(m_bakeEntities, m_fogVolume.Get());

    if (Result gatherResult = m_bakeData.GatherSceneData(); gatherResult.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to gather scene data for fog volume bake: {}", gatherResult.GetError().GetMessage());

        return;
    }

    m_bakeDataBuildTask = TaskSystem::GetInstance().Enqueue(
        [buildData = std::move(m_bakeData)]() mutable -> BakeData<FogVolume>
        {
            Result result = buildData.Build();

            if (result.HasError())
            {
                HYP_LOG(Lightmap, Error, "Failed to build fog volume occlusion data: {}", result.GetError().GetMessage());

                return {};
            }

            return std::move(buildData);
        },
        TaskThreadPoolName::THREAD_POOL_BACKGROUND);

    m_state = BakerState::Building;
}

void Baker<FogVolume>::OnBuildReady()
{
    AssertOnThread(g_simThread);

    m_bakeData = std::move(m_bakeDataBuildTask).Await();

    if (!m_bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Error, "Fog volume occlusion bake failed to build, skipping for {}", m_fogVolume->Id());

        return;
    }

    DispatchJobs();
}

void Baker<FogVolume>::HandleCompletedJob_Internal(BakeJobBase* job)
{
    HYP_SCOPE;

    BakeJob<FogVolume>* jobCasted = static_cast<BakeJob<FogVolume>*>(job);

    BakeData<FogVolume>& bakeData = jobCasted->GetBakeData();

    if (!bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for FogVolume {} is not built, skipping texture creation", m_fogVolume->Id());
        return;
    }

    typename BakeData<FogVolume>::VolumeBitmap& volumeBitmap = bakeData.GetVolumeBitmap();
    const typename BakeData<FogVolume>::NoiseBitmap& noiseBitmap = bakeData.GetNoiseBitmap();

    // update bitmap with texel data
    for (uint32 i = 0; i < uint32(bakeData.texels.Size()); i++)
    {
        const LightmapTexel& texel = bakeData.texels[i];

        volumeBitmap.SetPixel(
            i % volumeBitmap.GetWidth(),
            (i / volumeBitmap.GetWidth()) % volumeBitmap.GetHeight(),
            i / (volumeBitmap.GetWidth() * volumeBitmap.GetHeight()),
            texel.color0);
    }

    TextureDesc volumeTextureDesc {
        TextureType::Texture3D,
        volumeBitmap.GetFormat(),
        Vec3u { volumeBitmap.GetWidth(), volumeBitmap.GetHeight(), volumeBitmap.GetDepth() },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    Handle<Texture> volumeTexture = MakeHandle<Texture>(volumeTextureDesc, volumeBitmap.ToByteView());
    volumeTexture->SetName(NAME_FMT("FogVolume_{}_DataMap", m_fogVolume->GetName()));
    GetCurrentAssetRegistry()->PutAssetUnique(volumeTexture);

    TextureDesc noiseTextureDesc {
        TextureType::Texture3D,
        noiseBitmap.GetFormat(),
        Vec3u { noiseBitmap.GetWidth(), noiseBitmap.GetHeight(), noiseBitmap.GetDepth() },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_REPEAT
    };

    Handle<Texture> noiseTexture = MakeHandle<Texture>(noiseTextureDesc, noiseBitmap.ToByteView());
    noiseTexture->SetName(NAME_FMT("FogVolume_{}_NoiseMap", m_fogVolume->GetName()));
    GetCurrentAssetRegistry()->PutAssetUnique(noiseTexture);

    m_fogVolume->SetTextures(volumeTexture, noiseTexture);
}

} // namespace Baking
} // namespace Hyperion

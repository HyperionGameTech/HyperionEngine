/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <baking/fog_volume/FogVolumeBaker.hpp>
#include <baking/fog_volume/FogVolumeBakeJob.hpp>

#include <rendering/Texture.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

#include <scene/FogVolume.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

Baker<FogVolume>::Baker(BakerConfig&& config, const Handle<FogVolume>& fogVolume)
    : BakerBase(std::move(config), fogVolume, MakeStrongRef(fogVolume->GetScene()), fogVolume->GetWorldBounds()),
      m_fogVolume(fogVolume)
{
}

UniquePtr<BakeJobBase> Baker<FogVolume>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<FogVolume>>(std::move(params), m_fogVolume, &m_bakeData);
}

Result Baker<FogVolume>::Build_Internal()
{
    Assert(m_fogVolume != nullptr);

    m_bakeData = BakeData<FogVolume>(m_bakeEntities, m_fogVolume.Get());

    return m_bakeData.Build();
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

    g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Lightmaps", volumeTexture, AddAssetConflictMode::ReplaceExisting);

    CheckResult(volumeTexture->Create());

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
    g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Lightmaps", noiseTexture, AddAssetConflictMode::ReplaceExisting);
    
    CheckResult(noiseTexture->Create());

    m_fogVolume->SetTextures(volumeTexture, noiseTexture);
}

} // namespace Baking
} // namespace Hyperion

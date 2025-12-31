/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <baking/reflection_probe/ReflectionProbeBakeJob.hpp>

#include <rendering/Texture.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>
#include <rendering/asset/TextureAsset.hpp>

#include <scene/EnvProbe.hpp>

#include <engine/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

Baker<ReflectionProbe>::Baker(LightmapperConfig&& config, const Handle<ReflectionProbe>& envProbe)
    : BakerBase(std::move(config), envProbe, MakeStrongRef(envProbe->GetScene()), envProbe->GetWorldBounds()),
      m_envProbe(envProbe)
{
}

UniquePtr<BakeJobBase> Baker<ReflectionProbe>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<ReflectionProbe>>(std::move(params), m_envProbe, &m_bakeData);
}

Result Baker<ReflectionProbe>::Build_Internal()
{
    Assert(m_envProbe != nullptr);

    m_bakeData = BakeData<ReflectionProbe>(m_bakeEntities, m_envProbe.Get());

    return m_bakeData.Build();
}

void Baker<ReflectionProbe>::HandleCompletedJob_Internal(BakeJobBase* job)
{
    HYP_SCOPE;

    BakeJob<ReflectionProbe>* jobCasted = static_cast<BakeJob<ReflectionProbe>*>(job);

    const BakeData<ReflectionProbe>& bakeData = jobCasted->GetBakeData();

    if (!bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Lightmap data for EnvProbe {} is not built, skipping texture creation", m_envProbe->Id());
        return;
    }

    const Vec2u dimensions = m_envProbe->GetDimensions();

    // Convert lightmap data to bitmaps (6 faces stacked vertically)
    BakeData<ReflectionProbe>::BitmapType bitmap = bakeData.ToBitmap();

    TextureDesc textureDesc {
        TT_CUBEMAP,
        bitmap.GetFormat(),
        Vec3u { dimensions.x, dimensions.y, 1 },
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    TextureData textureData {
        ByteBuffer(bitmap.ToByteView())
    };

    Texture::GenerateMipmaps(textureDesc, textureData);

    Handle<Texture> cubemap = CreateObject<Texture>(textureDesc, std::move(textureData));

    cubemap->SetName(NAME_FMT("EnvProbe_{}_Baked", m_envProbe->GetUUID()));

    if (Result result = g_assetManager->GetAssetRegistry()->RegisterAsset("$Import/Media/Lightmaps", cubemap->GetAsset()).Await(); result.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to register radiance texture '{}' with asset registry: {}", cubemap->GetName(), result.GetError().GetMessage());
    }

    InitObject(cubemap);

    // Set the baked texture on the EnvProbe
    m_envProbe->SetBakedTexture(cubemap);

    HYP_LOG(Lightmap, Info, "EnvProbe {} lightmap baking complete! Radiance and irradiance textures created.", m_envProbe->Id());
}

} // namespace Baking
} // namespace Hyperion

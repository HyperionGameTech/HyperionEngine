/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/reflection_probe/ReflectionProbeBaker.hpp>
#include <baking/reflection_probe/ReflectionProbeBakeJob.hpp>

#include <rendering/Texture.hpp>

#include <asset/AssetRegistry.hpp>
#include <asset/Assets.hpp>

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
        TFM_LINEAR_MIPMAP,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    ByteBuffer imageData(bitmap.ToByteView());
    Texture::GenerateMipmaps(textureDesc, imageData);

    Handle<Texture> cubemap = MakeHandle<Texture>(textureDesc, imageData.ToByteView());
    cubemap->SetName(NAME_FMT("EnvProbe_{}_Baked", m_envProbe->GetName()));

    Result registerAssetResult = g_assetManager->GetAssetRegistry()->RegisterAsset(
        "$Import/Media/Lightmaps", cubemap, AddAssetConflictMode::ReplaceExisting);

    if (registerAssetResult.HasError())
    {
        HYP_LOG(Lightmap, Error, "Failed to register radiance texture '{}' with asset registry: {}",
            cubemap->GetName(), registerAssetResult.GetError().GetMessage());
    }

    InitObject(cubemap);

    // Set the baked texture on the EnvProbe
    m_envProbe->SetBakedTexture(cubemap);

    HYP_LOG(Lightmap, Info, "EnvProbe {} lightmap baking complete! Radiance and irradiance textures created.", m_envProbe->Id());
}

} // namespace Baking
} // namespace Hyperion

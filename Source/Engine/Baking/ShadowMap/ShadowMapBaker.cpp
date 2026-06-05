/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/ShadowMap/ShadowMapBaker.hpp>
#include <Baking/ShadowMap/ShadowMapBakeJob.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Texture.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Light.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {

namespace CoreApi {
CORE_API extern FilePath GetExecutablePath();
} // namespace CoreApi

namespace Baking {

Baker<Light>::Baker(BakerConfig&& config, const Handle<Light>& light)
    : BakerBase(std::move(config), light, MakeStrongRef(light->GetScene()), light->GetWorldBounds()),
      m_light(light)
{
}

UniquePtr<BakeJobBase> Baker<Light>::CreateJob(BakeJobParams&& params)
{
    return MakeUnique<BakeJob<Light>>(std::move(params), m_light, &m_bakeData);
}

void Baker<Light>::CreateLightmapRenderers()
{
    m_lightmapRenderers.Clear();

    if (!PerformsRayTracing())
    {
        return;
    }

    const uint32 maxTexelsPerFrame = MaxTexelsPerFrame();
    AssertDebug(maxTexelsPerFrame > 0);

    UniquePtr<ILightmapRenderer> lightmapRenderer = CreateRenderer(LightmapShadingType::SHADOW, maxTexelsPerFrame);

    if (lightmapRenderer != nullptr)
    {
        lightmapRenderer->Create();
        m_lightmapRenderers.PushBack(std::move(lightmapRenderer));
        return;
    }
}

Result Baker<Light>::Build_Internal()
{
    Assert(m_light != nullptr);
    InitObject(m_light);

    m_bakeData = BakeData<Light>(m_bakeEntities, m_light.Get());

    return m_bakeData.Build();
}

void Baker<Light>::OnCompleted_Internal()
{
    HYP_SCOPE;

    AssertDebug(m_bakeData.IsBuilt());
    if (!m_bakeData.IsBuilt())
    {
        HYP_LOG(Lightmap, Warning, "Shadow map bake data for Light {} is not built, skipping texture creation", m_light->Id());
        return;
    }

    const bool isCubemap = m_bakeData.GetNumFaces() == 6;

    auto bitmap = m_bakeData.ToBitmap();

    TextureDesc textureDesc {
        isCubemap ? TextureType::Cubemap : TextureType::Texture2D,
        TextureFormat::D16,
        Vec3u { bitmap.GetWidth(), isCubemap ? bitmap.GetHeight() / 6 : bitmap.GetHeight(), 1 },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    FileByteWriter tmpWriter { CoreApi::GetExecutablePath() / "TempShadow.bmp" };
    bitmap.Write(&tmpWriter);
    tmpWriter.Close();

    Assert(TextureUtils::BytesPerComponent(textureDesc.format) == TextureUtils::BytesPerComponent(bitmap.GetFormat()));

    Handle<Texture> shadowMap = MakeHandle<Texture>(textureDesc, bitmap.ToByteView());
    shadowMap->SetName(NAME_FMT("{}_BakedShadowMap", m_light->GetName()));
    CheckResult(shadowMap->Create());

    auto writeScope = m_light->GetWriteScope();
    m_light->SetBakedShadowMap(shadowMap);

    HYP_LOG(Lightmap, Verbose, "Shadow map baking for Light {} complete.", m_light->Id());
}

} // namespace Baking
} // namespace Hyperion
